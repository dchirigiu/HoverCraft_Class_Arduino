param(
  [Parameter(Mandatory=$true)][string]$ComPort,
  [int]$Seconds = 60
)
# Record UART output from the Nano to board_serial.log so it can be compared
# with the Wokwi simulation log (build/serial.log).
# Usage: powershell -ExecutionPolicy Bypass -File capture.ps1 COM5 60
$port = New-Object System.IO.Ports.SerialPort($ComPort, 9600, 'None', 8, 'One')
$port.ReadTimeout = 2000
$out = Join-Path $PSScriptRoot "board_serial.log"
try {
  $port.Open()
  Write-Output "Recording $ComPort at 9600 baud for $Seconds s -> $out  (Ctrl+C to stop early)"
  $deadline = (Get-Date).AddSeconds($Seconds)
  while ((Get-Date) -lt $deadline) {
    try { $line = $port.ReadLine() } catch [TimeoutException] { continue }
    Add-Content -Path $out -Value $line
  }
} catch {
  Write-Output "ERROR: $($_.Exception.Message)"
} finally {
  if ($port.IsOpen) { $port.Close() }
}
Write-Output "Done. Compare board_serial.log with the simulation's build/serial.log."
