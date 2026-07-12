// ESP32-C3 + MAX7219 8x32 Nightscout glucose display
//
// Wiring (see README.md for the full table):
//   MAX7219 VCC -> 5V   GND -> GND
//   MAX7219 DIN -> GPIO5   CLK -> GPIO4   CS -> GPIO6
//
// Before building: copy include/config.example.h to include/config.h
// and fill in your Wi-Fi credentials and Nightscout site.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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

struct GlucoseReading {
  bool valid = false;
  int sgv = 0;              // mg/dL
  String direction = "NONE";
  time_t epochSeconds = 0;
};

static GlucoseReading lastReading;
static unsigned long lastFetchMs = 0;
static char displayBuffer[64] = "CONNECTING";
static bool wifiWasConnected = false;
static bool alarmActive = false;
static bool invertOn = false;
static unsigned long lastBlinkMs = 0;
static const unsigned long BLINK_INTERVAL_MS = 500;

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
  P.displayText(displayBuffer, PA_LEFT, SCROLL_SPEED_MS, SCROLL_PAUSE_MS, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connect timed out, will keep retrying in background");
  }
}

static bool fetchGlucose(GlucoseReading &out) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String(NIGHTSCOUT_URL) + "/api/v1/entries.json?count=1";
  if (strlen(NIGHTSCOUT_TOKEN) > 0) {
    url += "&token=" + String(NIGHTSCOUT_TOKEN);
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

  bool low = reading.sgv <= LOW_THRESHOLD_MGDL;
  bool high = reading.sgv >= HIGH_THRESHOLD_MGDL;
  alarmActive = low || high || stale;

  char textBuf[64];
  snprintf(textBuf, sizeof(textBuf), "%s %s%s", valueBuf, trendSymbol(reading.direction), stale ? " OLD" : (low ? " LOW" : (high ? " HIGH" : "")));

  startScroll(textBuf);
}

void setup() {
  Serial.begin(115200);

  P.begin();
  P.setIntensity(DISPLAY_BRIGHTNESS);
  P.setTextAlignment(PA_CENTER);
  P.displayClear();
  startScroll("CONNECTING");

  connectWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  }
}

void loop() {
  if (P.displayAnimate()) {
    P.displayReset();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiWasConnected) {
      startScroll("WIFI LOST");
      wifiWasConnected = false;
    }
    connectWiFi(); // blocking retry; scrolling pauses for a few seconds while it runs
    if (WiFi.status() == WL_CONNECTED) {
      wifiWasConnected = true;
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      lastFetchMs = 0; // force an immediate fetch
    }
    return;
  }
  wifiWasConnected = true;

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
