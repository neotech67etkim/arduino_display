# One-time environment bootstrap for building this project with arduino-cli
# instead of the Arduino IDE GUI.
#
# Prerequisite: install arduino-cli first (not done here), e.g.:
#   winget install ArduinoSA.CLI
# or download from https://arduino.github.io/arduino-cli/latest/installation/

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    Write-Error "arduino-cli not found on PATH. Install it first (winget install ArduinoSA.CLI), then re-run this script."
    exit 1
}

arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

arduino-cli lib install "MD_Parola" "MD_MAX72XX" "ArduinoJson"

Write-Host ""
Write-Host "Setup complete."
Write-Host "Find your board's COM port with: arduino-cli board list"
Write-Host "Then build/upload with:  .\scripts\flash.ps1 -Port COM4"
