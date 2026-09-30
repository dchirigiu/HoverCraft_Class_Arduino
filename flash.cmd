@echo off
rem Flash the compiled sketch to the real Nano and open the serial monitor.
rem One-time setup: install the AVR toolchain with:  arduino-cli core install arduino:avr
rem Usage:   flash.cmd COM5              (new bootloader)
rem          flash.cmd COM5 oldbootloader (clone boards that fail to sync)
setlocal
rem Some Windows profiles have a broken/redirected Documents known-folder; pin
rem the sketchbook dir so arduino-cli never fails with "cannot get documents".
set "ARDUINO_DIRECTORIES_USER=%LOCALAPPDATA%\Arduino15\user"
where arduino-cli >nul 2>nul || (echo arduino-cli not in PATH - install with: winget install Arduino.Cli & exit /b 1)
if "%~1"=="" (
  arduino-cli board list
  echo.
  echo Usage: flash.cmd COM5 [oldbootloader]
  exit /b 0
)
set FQBN=arduino:avr:nano
if /i "%~2"=="oldbootloader" set FQBN=arduino:avr:nano:cpu=atmega328old
cd /d "%~dp0"
arduino-cli compile --fqbn %FQBN% --output-dir build sketch\hovercraft_ta1 || exit /b 1
arduino-cli upload -p %~1 --fqbn %FQBN% --input-dir build sketch\hovercraft_ta1 || exit /b 1
echo Flashed. Opening serial monitor at 9600 baud ^(
echo Ctrl+C to exit.
arduino-cli monitor -p %~1 --config baudrate=9600
