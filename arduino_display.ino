// ESP32-C3 + MAX7219 8x32 Nightscout glucose display
//
// Wiring (see README.md for the full table):
//   MAX7219 VCC -> 5V   GND -> GND
//   MAX7219 DIN -> GPIO5   CLK -> GPIO4   CS -> GPIO6
//
// Before building: copy config.example.h to config.h (same folder as this .ino).
// Wi-Fi/Nightscout/display settings there are only the *initial* defaults —
// once saved through the web config page they live in flash (NVS) and take
// priority. See README.md "Web Configuration" for how to reach the page.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include "config.h"

// Individually-chained 8x8 breakout modules (separate VCC/GND/DIN/CS/CLK
// headers, jumpered DOUT->DIN between boards) typically need GENERIC_HW.
// A fused 4-in-1 "FC-16" board needs FC16_HW instead. If text shows
// mirrored, upside-down, or out of order, try PAROLA_HW or ICSTATION_HW.
#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define MAX_DEVICES   4

#define CLK_PIN  4
#define DATA_PIN 5
#define CS_PIN   6

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
WebServer webServer(80);
Preferences prefs;

struct Settings {
  String ssid;
  String password;
  String nsUrl;
  String nsToken;
  int lowThreshold;
  int highThreshold;
  int brightness;
  int scrollSpeed;
};

static Settings settings;

struct GlucoseReading {
  bool valid = false;
  int sgv = 0;              // mg/dL
  String direction = "NONE";
  time_t epochSeconds = 0;
};

static GlucoseReading lastReading;
static unsigned long lastFetchMs = 0;
static char displayBuffer[64] = "CONNECTING";
static bool staConnected = false;
static bool alarmActive = false;
static bool invertOn = false;
static unsigned long lastBlinkMs = 0;
static const unsigned long BLINK_INTERVAL_MS = 500;
static unsigned long lastStaAttemptMs = 0;
static const unsigned long STA_RETRY_INTERVAL_MS = 30000;
static bool restartPending = false;
static unsigned long restartAtMs = 0;

static void loadSettings() {
  prefs.begin("glucosecfg", true);
  settings.ssid = prefs.getString("ssid", WIFI_SSID);
  settings.password = prefs.getString("pass", WIFI_PASSWORD);
  settings.nsUrl = prefs.getString("nsUrl", NIGHTSCOUT_URL);
  settings.nsToken = prefs.getString("nsToken", NIGHTSCOUT_TOKEN);
  settings.lowThreshold = prefs.getInt("low", LOW_THRESHOLD_MGDL);
  settings.highThreshold = prefs.getInt("high", HIGH_THRESHOLD_MGDL);
  settings.brightness = prefs.getInt("bright", DISPLAY_BRIGHTNESS);
  settings.scrollSpeed = prefs.getInt("scroll", SCROLL_SPEED_MS);
  prefs.end();
}

static void saveSettings() {
  prefs.begin("glucosecfg", false);
  prefs.putString("ssid", settings.ssid);
  prefs.putString("pass", settings.password);
  prefs.putString("nsUrl", settings.nsUrl);
  prefs.putString("nsToken", settings.nsToken);
  prefs.putInt("low", settings.lowThreshold);
  prefs.putInt("high", settings.highThreshold);
  prefs.putInt("bright", settings.brightness);
  prefs.putInt("scroll", settings.scrollSpeed);
  prefs.end();
}

static String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&':  out += "&amp;";  break;
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '"':  out += "&quot;"; break;
      default:   out += c;
    }
  }
  return out;
}

static const char *trendSymbol(const String &direction) {
  if (direction == "DoubleUp") return "^^";
  if (direction == "SingleUp") return "^";
  if (direction == "FortyFiveUp") return "/";
  if (direction == "Flat") return "-";
  if (direction == "FortyFiveDown") return "\\";
  if (direction == "SingleDown") return "v";
  if (direction == "DoubleDown") return "vv";
  return "?";
}

static void startScroll(const char *text) {
  strncpy(displayBuffer, text, sizeof(displayBuffer) - 1);
  displayBuffer[sizeof(displayBuffer) - 1] = '\0';
  P.displayText(displayBuffer, PA_LEFT, settings.scrollSpeed, SCROLL_PAUSE_MS, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

// Starts the always-on AP (for reaching the config page) plus, if
// credentials are saved, an initial bounded attempt to join the home
// network. Later reconnects happen in the background via serviceWiFi().
static void startWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("Config AP started: ");
  Serial.print(AP_SSID);
  Serial.print(" @ ");
  Serial.println(WiFi.softAPIP());

  if (settings.ssid.length() == 0) {
    Serial.println("No saved Wi-Fi SSID yet, waiting for config page");
    return;
  }

  Serial.print("Connecting to ");
  Serial.println(settings.ssid);
  WiFi.begin(settings.ssid.c_str(), settings.password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(250);
  }
  lastStaAttemptMs = millis();
}

// Non-blocking: call every loop() to notice connect/disconnect transitions
// and periodically retry in the background without freezing the display.
static void serviceWiFi() {
  bool nowConnected = (WiFi.status() == WL_CONNECTED);

  if (nowConnected && !staConnected) {
    staConnected = true;
    Serial.print("Wi-Fi connected, IP: ");
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lastFetchMs = 0; // force an immediate fetch
  } else if (!nowConnected && staConnected) {
    staConnected = false;
    startScroll("WIFI LOST");
  }

  if (!nowConnected && settings.ssid.length() > 0 &&
      millis() - lastStaAttemptMs >= STA_RETRY_INTERVAL_MS) {
    lastStaAttemptMs = millis();
    Serial.println("Retrying Wi-Fi connection");
    WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
  }
}

static bool fetchGlucose(GlucoseReading &out) {
  if (WiFi.status() != WL_CONNECTED || settings.nsUrl.length() == 0) return false;

  String url = settings.nsUrl + "/api/v1/entries.json?count=1";
  if (settings.nsToken.length() > 0) {
    url += "&token=" + settings.nsToken;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Nightscout hosts vary widely; not pinning a CA here.

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("HTTPClient begin() failed");
    return false;
  }

  int httpCode = https.GET();
  bool ok = false;

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err && doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
      JsonObject entry = doc.as<JsonArray>()[0];
      out.sgv = entry["sgv"] | 0;
      out.direction = String((const char *)(entry["direction"] | "NONE"));
      out.epochSeconds = (time_t)((entry["date"] | 0LL) / 1000);
      out.valid = out.sgv > 0;
      ok = out.valid;
    } else {
      Serial.println("Failed to parse Nightscout response");
    }
  } else {
    Serial.printf("Nightscout HTTP GET failed, code=%d\n", httpCode);
  }

  https.end();
  return ok;
}

static void updateDisplayText(const GlucoseReading &reading) {
  alarmActive = false;

  if (!reading.valid) {
    startScroll("NO DATA");
    return;
  }

  time_t now = time(nullptr);
  long minutesAgo = (now > 0 && reading.epochSeconds > 0)
                         ? (long)((now - reading.epochSeconds) / 60)
                         : -1;
  bool stale = (minutesAgo >= 0) && (minutesAgo >= STALE_MINUTES);

  char valueBuf[8];
#if USE_MMOL
  float mmol = reading.sgv / 18.0182f;
  snprintf(valueBuf, sizeof(valueBuf), "%.1f", mmol);
#else
  snprintf(valueBuf, sizeof(valueBuf), "%d", reading.sgv);
#endif

  bool low = reading.sgv <= settings.lowThreshold;
  bool high = reading.sgv >= settings.highThreshold;
  alarmActive = low || high || stale;

  char textBuf[64];
  snprintf(textBuf, sizeof(textBuf), "%s %s%s", valueBuf, trendSymbol(reading.direction), stale ? " OLD" : (low ? " LOW" : (high ? " HIGH" : "")));

  startScroll(textBuf);
}

static String buildConfigPage() {
  String statusLine;
  if (staConnected) {
    statusLine = "Wi-Fi connected, IP " + WiFi.localIP().toString();
  } else {
    statusLine = "Not connected to Wi-Fi yet - device keeps retrying in the background";
  }
  if (lastReading.valid) {
    statusLine += " | Last reading: " + String(lastReading.sgv) + " mg/dL";
  }

  String html;
  html.reserve(2200);
  html += "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>";
  html += "<title>Glucose Display Setup</title>";
  html += "<style>body{font-family:sans-serif;max-width:480px;margin:1.5em auto;padding:0 1em}"
          "label{display:block;margin-top:0.8em;font-weight:bold}"
          "input{width:100%;box-sizing:border-box;padding:0.4em;font-size:1em}"
          "button{margin-top:1.2em;padding:0.6em 1.2em;font-size:1em}"
          ".status{background:#eef;padding:0.6em;border-radius:4px;font-size:0.9em}</style>";
  html += "</head><body>";
  html += "<h2>Glucose Display Setup</h2>";
  html += "<p class=status>" + htmlEscape(statusLine) + "</p>";
  html += "<form method='POST' action='/save'>";

  html += "<label>Wi-Fi SSID</label><input name='ssid' value='" + htmlEscape(settings.ssid) + "'>";
  html += "<label>Wi-Fi Password (leave blank to keep current)</label><input name='password' type='password' placeholder='(unchanged)'>";

  html += "<label>Nightscout URL</label><input name='nsUrl' value='" + htmlEscape(settings.nsUrl) + "' placeholder='https://your-site.example.com'>";
  html += "<label>Nightscout Token (optional)</label><input name='nsToken' value='" + htmlEscape(settings.nsToken) + "'>";

  html += "<label>Low alarm threshold (mg/dL)</label><input name='low' type='number' min='40' max='400' value='" + String(settings.lowThreshold) + "'>";
  html += "<label>High alarm threshold (mg/dL)</label><input name='high' type='number' min='40' max='400' value='" + String(settings.highThreshold) + "'>";

  html += "<label>Brightness (0-15)</label><input name='bright' type='number' min='0' max='15' value='" + String(settings.brightness) + "'>";
  html += "<label>Scroll speed (ms/column, lower = faster)</label><input name='scroll' type='number' min='10' max='500' value='" + String(settings.scrollSpeed) + "'>";

  html += "<button type='submit'>Save &amp; Restart</button>";
  html += "</form></body></html>";
  return html;
}

static void handleRoot() {
  webServer.send(200, "text/html", buildConfigPage());
}

static int clampInt(const String &value, int fallback, int lo, int hi) {
  if (value.length() == 0) return fallback;
  int v = value.toInt();
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void handleSave() {
  if (webServer.hasArg("ssid")) settings.ssid = webServer.arg("ssid");
  String newPassword = webServer.arg("password");
  if (newPassword.length() > 0) settings.password = newPassword;
  if (webServer.hasArg("nsUrl")) settings.nsUrl = webServer.arg("nsUrl");
  if (webServer.hasArg("nsToken")) settings.nsToken = webServer.arg("nsToken");

  settings.lowThreshold = clampInt(webServer.arg("low"), settings.lowThreshold, 40, 400);
  settings.highThreshold = clampInt(webServer.arg("high"), settings.highThreshold, 40, 400);
  settings.brightness = clampInt(webServer.arg("bright"), settings.brightness, 0, 15);
  settings.scrollSpeed = clampInt(webServer.arg("scroll"), settings.scrollSpeed, 10, 500);

  saveSettings();

  String html = "<!DOCTYPE html><html><head><meta http-equiv=refresh content='3;url=/'>"
                 "<title>Saved</title></head><body>"
                 "<p>Settings saved. Restarting the device now...</p></body></html>";
  webServer.send(200, "text/html", html);

  restartPending = true;
  restartAtMs = millis() + 1000; // let the response flush before rebooting
}

static void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.onNotFound([]() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
  });
  webServer.begin();
}

void setup() {
  Serial.begin(115200);
  loadSettings();

  P.begin();
  P.setIntensity(settings.brightness);
  P.setTextAlignment(PA_CENTER);
  P.displayClear();
  startScroll("CONNECTING");

  startWiFi();
  setupWebServer();

  if (WiFi.status() == WL_CONNECTED) {
    staConnected = true;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  } else {
    char setupMsg[96];
    snprintf(setupMsg, sizeof(setupMsg), "SETUP: WiFi '%s' -> %s", AP_SSID, WiFi.softAPIP().toString().c_str());
    startScroll(setupMsg);
  }
}

void loop() {
  webServer.handleClient();

  if (P.displayAnimate()) {
    P.displayReset();
  }

  if (restartPending && millis() >= restartAtMs) {
    ESP.restart();
  }

  serviceWiFi();

  if (!staConnected) {
    return; // no home Wi-Fi yet; config AP + web server above still run
  }

  unsigned long nowMs = millis();
  if (lastFetchMs == 0 || nowMs - lastFetchMs >= REFRESH_INTERVAL_MS) {
    lastFetchMs = nowMs;

    GlucoseReading reading;
    if (fetchGlucose(reading)) {
      lastReading = reading;
    } else if (!lastReading.valid) {
      startScroll("FETCH ERR");
    }

    if (lastReading.valid) {
      updateDisplayText(lastReading);
    }
  }

  if (alarmActive) {
    if (nowMs - lastBlinkMs >= BLINK_INTERVAL_MS) {
      lastBlinkMs = nowMs;
      invertOn = !invertOn;
      P.setInvert(invertOn);
    }
  } else if (invertOn) {
    invertOn = false;
    P.setInvert(false);
  }
}
