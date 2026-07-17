param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Fqbn = "esp32:esp32:esp32c3:CDCOnBoot=cdc",
    [switch]$NoMonitor
)

arduino-cli compile --upload -p $Port --fqbn $Fqbn "$PSScriptRoot\.."
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $NoMonitor) {
    arduino-cli monitor -p $Port -c baudrate=115200
}
