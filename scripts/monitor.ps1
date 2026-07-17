param(
    [Parameter(Mandatory = $true)][string]$Port,
    [int]$Baud = 115200
)

arduino-cli monitor -p $Port -c baudrate=$Baud
