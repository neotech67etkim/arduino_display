#pragma once

// Copy this file to "config.h" (same folder) and fill in your own values.
// config.h is gitignored so your credentials never get committed.
//
// Wi-Fi / Nightscout / display values below are only *initial* defaults for
// the very first boot. Once you save settings through the web config page
// (see README.md "Web Configuration"), they're stored in flash (NVS) and
// override everything here — editing config.h after that has no effect
// unless you erase the device's flash.

// ---- Wi-Fi (initial defaults only, see note above) ----
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Always-on config access point. Connect to this SSID any time (even
// alongside your home Wi-Fi) to reach the setup page at 192.168.4.1.
// Password must be 8+ characters (WPA2 minimum).
#define AP_SSID     "GlucoseDisplay-Setup"
#define AP_PASSWORD "glucose123"

// ---- Nightscout (initial defaults only, see note above) ----
// Must start with "https://", no trailing slash.
#define NIGHTSCOUT_URL "https://your-site.example.com"
// Optional read-only API token (Nightscout Admin Tools -> Subjects).
// Leave as "" if your site does not require one.
#define NIGHTSCOUT_TOKEN ""

// ---- Behavior ----
// How often to poll Nightscout, in milliseconds.
#define REFRESH_INTERVAL_MS 60000UL

// Consider a reading stale (sensor/upload may have stopped) after this many minutes.
#define STALE_MINUTES 15

// Alarm thresholds, in mg/dL (initial defaults only, editable on the web page).
#define LOW_THRESHOLD_MGDL  70
#define HIGH_THRESHOLD_MGDL 180

// Show values in mmol/L instead of mg/dL.
#define USE_MMOL 0

// ---- Display (initial defaults only, editable on the web page) ----
// LED matrix brightness, 0 (dim) - 15 (max).
#define DISPLAY_BRIGHTNESS 5

// Scroll speed in ms/column (lower = faster) and pause between scrolls in ms.
#define SCROLL_SPEED_MS 40
#define SCROLL_PAUSE_MS 1000
