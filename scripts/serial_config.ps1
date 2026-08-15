# Minimal interactive serial terminal for the config screen - no Arduino IDE
# or arduino-cli needed, just PowerShell (built into Windows).
#
# Usage:
#   .\scripts\serial_config.ps1 -Port COM5
#
# Type commands and press Enter, e.g.:
#   ssid=YourWifiName
#   pass=YourWifiPassword
#   SAVE
# Type 'exit' to close this terminal (the device keeps running).

param(
    [Parameter(Mandatory = $true)][string]$Port,
    [int]$Baud = 115200
)

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 500
$sp.NewLine = "`n"

try {
    $sp.Open()
} catch {
    Write-Error "Could not open ${Port}: $_"
    exit 1
}

function Drain-Serial {
    while ($true) {
        try {
            $line = $sp.ReadLine()
            Write-Host $line
        } catch [System.TimeoutException] {
            break
        }
    }
}

Write-Host "Connected to $Port at $Baud baud."
Write-Host "Type a command and press Enter (e.g. ssid=MyWifi, pass=secret, SAVE)."
Write-Host "Type 'exit' to quit this terminal - the device keeps running."
Write-Host ""

# Show whatever the device already printed (boot banner / config screen).
Start-Sleep -Milliseconds 300
Drain-Serial

while ($true) {
    $cmd = Read-Host ">"
    if ($cmd -eq "exit") { break }
    $sp.WriteLine($cmd)
    Start-Sleep -Milliseconds 300
    Drain-Serial
}

$sp.Close()
Write-Host "Disconnected."
