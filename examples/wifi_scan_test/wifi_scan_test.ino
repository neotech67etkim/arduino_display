// Minimal WiFi hardware diagnostic: scans and prints all nearby networks.
// Use this to check whether the radio can see ANY networks at all,
// independent of any SSID/password configuration - if this comes back
// empty near known-good networks, suspect a hardware/antenna fault
// rather than a code or credentials issue.

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[scan test] starting");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
}

void loop() {
  Serial.println("[scan test] scanning...");
  int n = WiFi.scanNetworks();
  Serial.printf("[scan test] found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %2d: %-32s RSSI=%d ch=%d\n",
                  i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
  }
  WiFi.scanDelete();
  delay(5000);
}
