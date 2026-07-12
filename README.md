# ESP32-C3 Glucose Display

Wall/desk display that polls a [Nightscout](https://nightscout.github.io/) site
over Wi-Fi and shows the latest glucose reading + trend arrow on a MAX7219
8x32 LED matrix (4x chained 8x8 modules).

This is an informational display only — **not a medical device**. Always
confirm readings on your CGM app/receiver before making treatment decisions.

## Hardware

- ESP32-C3 dev board (e.g. ESP32-C3-DevKitM-1 / DevKitC-02, "Super Mini", etc.)
- MAX7219 8x32 LED dot matrix module (4 chained 8x8 "FC-16" style modules)
- 5V power supply capable of ~500mA+ for the matrix at full brightness

## Wiring

| MAX7219 module | ESP32-C3 pin |
|-----------------|--------------|
| VCC             | 5V           |
| GND             | GND          |
| DIN             | GPIO5        |
| CS              | GPIO6        |
| CLK             | GPIO4        |

Notes:
- Power the matrix from 5V (e.g. the board's USB 5V pin), not 3V3 — a 32-LED
  matrix can pull more current than the onboard 3.3V regulator supplies.
  Share a common GND with the ESP32-C3.
- Pins are defined in `src/main.cpp` (`CLK_PIN`, `DATA_PIN`, `CS_PIN`) and can
  be changed to any free GPIO if GPIO4/5/6 are already in use on your board.
- If the text appears mirrored, upside down, or the modules are in the wrong
  order, change `HARDWARE_TYPE` in `src/main.cpp` between `FC16_HW` (most
  common for cheap 4-in-1 boards), `PAROLA_HW`, `GENERIC_HW`, or `ICSTATION_HW`
  until it renders correctly.

## Software setup

This is a [PlatformIO](https://platformio.org/) project.

1. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
   for VS Code, or the `pio` CLI.
2. Copy `include/config.example.h` to `include/config.h` and fill in:
   - `WIFI_SSID` / `WIFI_PASSWORD`
   - `NIGHTSCOUT_URL` — your site, starting with `https://`, no trailing slash
   - `NIGHTSCOUT_TOKEN` — a read-only token if your site requires one
     (Nightscout Admin Tools -> Subjects). Leave `""` if not needed.
   - Alarm thresholds, refresh interval, mg/dL vs mmol/L, brightness, etc.

   `include/config.h` is gitignored so your credentials never get committed.
3. Connect the ESP32-C3 over USB and build/upload:

   ```sh
   pio run -t upload
   pio device monitor
   ```

## Behavior

- Scrolls `<value> <trend arrow>` continuously, e.g. `132 ^` or `98 -`.
- Trend arrows: `^^` double up, `^` up, `/` rising, `-` flat, `\` falling,
  `v` down, `vv` double down.
- Appends `LOW` / `HIGH` when outside the configured thresholds, or `OLD`
  when the last Nightscout entry is older than `STALE_MINUTES` (sensor/upload
  may have stopped). In any of these states the display blinks.
- Automatically reconnects Wi-Fi if the connection drops.

## Libraries used

- [MD_Parola](https://github.com/MajicDesigns/MD_Parola) / [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) — LED matrix driver + text/scrolling
- [ArduinoJson](https://arduinojson.org/) — parsing the Nightscout API response

Both are pulled automatically by PlatformIO via `lib_deps` in `platformio.ini`.
