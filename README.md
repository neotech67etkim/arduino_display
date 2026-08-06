# ESP32-C3 Glucose Display

Wall/desk display that polls a [Nightscout](https://nightscout.github.io/) site
over Wi-Fi and shows the latest glucose reading + trend arrow on a MAX7219
8x32 LED matrix (4x chained 8x8 modules).

This is an informational display only — **not a medical device**. Always
confirm readings on your CGM app/receiver before making treatment decisions.

## Hardware

- ESP32-C3 dev board — this project is being developed against an
  **ESP32-C3 "Super Mini"** board, pinout below
- MAX7219 8x8 LED dot matrix modules, individually chained (separate
  VCC/GND/DIN/CS/CLK and VCC/GND/DOUT/CS/CLK headers) — 4 of them chained
  make the 8x32 display
- 5V power supply capable of ~500mA+ for the matrix at full brightness

### ESP32-C3 Super Mini pinout

From the board's reference pinout diagram, USB-C at the top:

| Left pin | Function      | | Right pin | Function        |
|----------|---------------|-|-----------|-----------------|
| 5V       | 5V            | | 5         | GPIO5 (A5, MISO)|
| G        | GND           | | 6         | GPIO6 (MOSI)    |
| 3.3      | 3V3           | | 7         | GPIO7 (SS)      |
| 4        | GPIO4 (A4, SCK)| | 8        | GPIO8 (SDA)     |
| 3        | GPIO3 (A3)    | | 9         | GPIO9 (SCL)     |
| 2        | GPIO2 (A2)    | | 10        | GPIO10          |
| 1        | GPIO1 (A1)    | | 20        | GPIO20 (RX)     |
| 0        | GPIO0 (A0)    | | 21        | GPIO21 (TX)     |

`BOOT` and `RST` buttons sit next to the USB-C port. Notes for this board:
- `GPIO9` is the BOOT strapping pin (also labeled SCL by default) — don't
  wire anything to it, it selects boot mode at power-up.
- `GPIO8` is the default SDA pin and on many Super Mini revisions also
  drives the onboard WS2812 status LED — avoid it for other peripherals.
- `GPIO0`-`GPIO5` double as analog inputs (A0-A5) if this project ever needs
  a button, buzzer, or sensor.
- `GPIO20`/`GPIO21` are the UART0 RX/TX pins.
- If a first upload doesn't get recognized, hold `BOOT`, tap `RST`, then
  release `BOOT` to force download mode before retrying.

## Wiring

| MAX7219 module | ESP32-C3 pin |
|-----------------|--------------|
| VCC             | 5V           |
| GND             | GND (`G`)    |
| DIN             | GPIO6        |
| CS              | GPIO7        |
| CLK             | GPIO4        |

Notes:
- Power the matrix from the board's `5V` pin, not `3.3` — a 32-LED matrix can
  pull more current than the onboard 3.3V regulator supplies. Share a common
  ground with the ESP32-C3.
- Pins are defined in `arduino_display.ino` (`CLK_PIN`, `DATA_PIN`, `CS_PIN`)
  and can be changed to any free GPIO if GPIO4/6/7 are already in use on your
  board (avoid GPIO8/GPIO9 per the notes above).
- If the text appears mirrored, upside down, or the modules are in the wrong
  order, change `HARDWARE_TYPE` in `arduino_display.ino` between `GENERIC_HW`
  (default here, for individually-chained 8x8 breakout modules), `FC16_HW`
  (fused 4-in-1 boards), `PAROLA_HW`, or `ICSTATION_HW` until it renders
  correctly.

## Examples

`examples/wifi_text_scroll/` — a standalone demo: type a message into a web
page served by the ESP32-C3 and it scrolls on the matrix. Useful for
bring-up/testing the display and wiring before running the full Nightscout
client. Open `examples/wifi_text_scroll/wifi_text_scroll.ino` directly in
the Arduino IDE.

## Software setup

This is an Arduino IDE project — `arduino_display.ino` is the main sketch
(the folder name must match the `.ino` filename, which is why the repo is
laid out this way).

1. In the Arduino IDE, install ESP32 board support: **File > Preferences**,
   add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   to "Additional boards manager URLs", then **Tools > Board > Boards
   Manager**, search `esp32`, install the Espressif package.
2. **Tools > Board** → select `ESP32C3 Dev Module`.
3. **Tools > Library Manager**, install:
   - `MD_Parola` (and its dependency `MD_MAX72XX`, both by majicDesigns)
   - `ArduinoJson` (by Benoit Blanchon)
4. Open `arduino_display.ino` (**File > Open**, browse to this repo folder).
5. Copy `config.example.h` to `config.h` (same folder, appears as a second
   tab in the IDE). `config.h` is gitignored so nothing you put in it gets
   committed.

   Everything in it (Wi-Fi, Nightscout URL/token, thresholds, brightness,
   scroll speed) is only the **first-boot default** — the real, persistent
   copy of these settings lives in flash and is edited from the web config
   page (below) without ever re-flashing the device. You can leave
   `WIFI_SSID`/`WIFI_PASSWORD`/`NIGHTSCOUT_URL` blank in `config.h` and set
   everything from the web page instead if you'd rather not put Wi-Fi
   credentials in a file at all.
6. **Tools > USB CDC On Boot** → `Enabled` (required on native-USB boards
   like the Super Mini for `Serial`/Serial Monitor to work at all).
7. Upload, then **Tools > Serial Monitor** at `115200` baud to watch it boot.

### Command-line alternative (arduino-cli)

If you'd rather skip the IDE entirely — e.g. to drive the whole
compile/upload/debug loop from a terminal, or so a locally-running Claude
Code session can do it directly — `scripts/` has PowerShell wrappers around
[arduino-cli](https://arduino.github.io/arduino-cli/):

```powershell
.\scripts\setup.ps1              # one-time: installs ESP32 core + libraries
arduino-cli board list           # find your COM port
.\scripts\flash.ps1 -Port COM4   # compile, upload, open serial monitor
```

See `CLAUDE.md` for the full command reference and project-specific gotchas
(USB CDC On Boot, wiring, hardware type, etc.) that a Claude Code session
running on your machine would want up front.

## Web Configuration

The device always runs its own Wi-Fi access point in addition to (optionally)
being joined to your home network, so the config page is reachable no matter
what:

1. Connect your phone/laptop to Wi-Fi network **`GlucoseDisplay-Setup`**
   (password `glucose123` — change `AP_SSID`/`AP_PASSWORD` in `config.h`
   before flashing if you want different ones).
2. Open **`http://192.168.4.1`** in a browser.
3. Fill in your home Wi-Fi SSID/password, Nightscout URL/token, alarm
   thresholds, brightness, and scroll speed, then **Save & Restart**.

The device reboots, joins your home Wi-Fi, and starts polling Nightscout —
while still keeping the `GlucoseDisplay-Setup` AP up, so you can always get
back to this page (e.g. `http://192.168.4.1`, or the device's normal IP
shown at the top of the page) to change settings later without re-flashing.

If the saved Wi-Fi credentials ever stop working (password changed, moved
to a new router), the matrix scrolls a `SETUP: WiFi '...' -> 192.168.4.1`
message and keeps retrying the saved network in the background — connect to
the AP and update the credentials the same way.

## Behavior

- Scrolls `<value> <trend arrow>` continuously, e.g. `132 ^` or `98 -`.
- Trend arrows: `^^` double up, `^` up, `/` rising, `-` flat, `\` falling,
  `v` down, `vv` double down.
- Appends `LOW` / `HIGH` when outside the configured thresholds, or `OLD`
  when the last Nightscout entry is older than `STALE_MINUTES` (sensor/upload
  may have stopped). In any of these states the display blinks.
- Automatically retries Wi-Fi in the background if the connection drops,
  without freezing the display.

## Libraries used

- [MD_Parola](https://github.com/MajicDesigns/MD_Parola) / [MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX) — LED matrix driver + text/scrolling (install via Library Manager)
- [ArduinoJson](https://arduinojson.org/) — parsing the Nightscout API response (install via Library Manager)
- `WiFi` / `WebServer` / `Preferences` / `HTTPClient` — bundled with the ESP32
  board package, no separate install needed
