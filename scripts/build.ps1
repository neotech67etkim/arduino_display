param(
    # If your core's USB CDC On Boot option key/value differs, run
    # `arduino-cli board details -b esp32:esp32:esp32c3` to find the right
    # one and pass -Fqbn accordingly.
    [string]$Fqbn = "esp32:esp32:esp32c3:CDCOnBoot=cdc"
)

arduino-cli compile --fqbn $Fqbn "$PSScriptRoot\.."
