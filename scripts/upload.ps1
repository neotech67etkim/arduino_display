param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Fqbn = "esp32:esp32:esp32c3:CDCOnBoot=cdc"
)

arduino-cli upload -p $Port --fqbn $Fqbn "$PSScriptRoot\.."
