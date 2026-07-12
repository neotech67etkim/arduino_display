#pragma once

// Copy this file to "config.h" (same folder) and fill in your own values.
// config.h is gitignored so your credentials never get committed.

// ---- Wi-Fi ----
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ---- Nightscout ----
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

// Alarm thresholds, in mg/dL.
#define LOW_THRESHOLD_MGDL  70
#define HIGH_THRESHOLD_MGDL 180

// Show values in mmol/L instead of mg/dL.
#define USE_MMOL 0

// ---- Display ----
// LED matrix brightness, 0 (dim) - 15 (max).
#define DISPLAY_BRIGHTNESS 5

// Scroll speed in ms/column (lower = faster) and pause between scrolls in ms.
#define SCROLL_SPEED_MS 40
#define SCROLL_PAUSE_MS 1000
