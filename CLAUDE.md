# ESP32-C3 Glucose Display

Arduino IDE sketch (not PlatformIO) for an ESP32-C3 "Super Mini" board driving
a MAX7219 8x32 LED matrix, polling a Nightscout site for glucose readings.
`arduino_display.ino` is the main sketch (repo root folder name matches the
`.ino` filename, which Arduino tooling requires).

## Build / upload / monitor (arduino-cli)

This project can be built headlessly with [arduino-cli](https://arduino.github.io/arduino-cli/)
instead of the Arduino IDE GUI — useful for driving the whole compile/upload/
debug loop directly from a shell instead of clicking through menus.

One-time setup (installs the ESP32 core + required libraries):
```powershell
.\scripts\setup.ps1
```

Find the board's COM port:
```powershell
arduino-cli board list
```

Compile only:
```powershell
.\scripts\build.ps1
```

Upload only (board already compiled, or let arduino-cli compile+upload with `flash.ps1`):
```powershell
.\scripts\upload.ps1 -Port COM4
```

Compile, upload, and open the serial monitor in one step (the normal
edit-flash-check loop):
```powershell
.\scripts\flash.ps1 -Port COM4
```

FQBN is `esp32:esp32:esp32c3:CDCOnBoot=cdc`. If a build/upload fails on the
`CDCOnBoot` option, run `arduino-cli board details -b esp32:esp32:esp32c3` to
see the exact option key/value for the installed core version and pass
`-Fqbn` accordingly to the scripts above.

## Required libraries

Installed by `scripts/setup.ps1`, or manually via Library Manager: `MD_Parola`,
`MD_MAX72XX`, `ArduinoJson`. `WiFi`/`WebServer`/`Preferences`/`HTTPClient` are
bundled with the ESP32 board core.

## Before building

`config.h` must exist next to `arduino_display.ino` (copy `config.example.h`
→ `config.h`; it's gitignored). Its values are only first-boot defaults —
real settings (Wi-Fi, Nightscout URL/token, thresholds, brightness, scroll
speed) live in flash (NVS) and are edited from the device's own web config
page once it's running (see README "Web Configuration").

## Known gotchas (already hit these once, don't re-derive)

- **USB CDC On Boot must be Enabled** (or `CDCOnBoot=cdc` in the FQBN) — this
  board has no USB-serial bridge chip; without it, `Serial`/Serial Monitor
  produces nothing at all, which looks like a hang but isn't.
- `HARDWARE_TYPE` is `MD_MAX72XX::GENERIC_HW` — the actual hardware is
  individually-chained 8x8 breakout modules (separate DIN/DOUT headers), not
  a fused 4-in-1 "FC-16" board. If display output is mirrored/reordered, try
  `FC16_HW`/`PAROLA_HW`/`ICSTATION_HW` instead.
- Wiring is CLK→GPIO4, DIN→GPIO6, CS→GPIO7 (software/bit-banged SPI — the
  ESP32-C3 has no VSPI/HSPI split like classic ESP32, so arbitrary GPIOs are
  used rather than hardware-SPI default pins).
- Never hot-plug the DIN/CLK/CS wires while the board is powered — it can
  glitch the MAX7219's internal registers into a state that a plain
  microcontroller reset (RST) won't clear, because MAX7219 has no reset pin
  of its own (only full power-cycle or a clean re-sent SPI init clears it).
- `examples/wifi_text_scroll/` is a standalone bring-up sketch (not built by
  the scripts above) for testing display + Wi-Fi independent of the
  Nightscout/config-page logic.
