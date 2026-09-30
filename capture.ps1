param(
  [Parameter(Mandatory=$true)][string]$ComPort,
  [int]$Seconds = 60
)
# Record UART output from the Nano to board_serial.log so it can be compared
# with the Wokwi simulation log (build/serial.log). Each run starts a new log,
# so rename the file between trials. Close any other serial monitor first:
# only one program can open the COM port at a time.
# Usage: powershell -ExecutionPolicy Bypass -File capture.ps1 COM5 60
$port = New-Object System.IO.Ports.SerialPort($ComPort, 9600, 'None', 8, 'One')
$port.ReadTimeout = 2000
$port.NewLine = "`r`n"   # Serial.println() ends lines with CR LF
$port.DtrEnable = $true  # opening the port resets the Nano, so the log starts at "TA1 READY"
$out = Join-Path $PSScriptRoot "board_serial.log"
try {
  $port.Open()
  [System.IO.File]::WriteAllText($out, '')   # fresh log for this run
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
