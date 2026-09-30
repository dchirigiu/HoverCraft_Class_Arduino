@echo off
rem Build the BOARD version of the sketch (-DON_BOARD, so it uses VREF_MV_BOARD),
rem upload it to the Nano, then open the serial monitor. Output goes to
rem build-board\ so the simulator build in build\ stays untouched.
rem In the VS Code terminal (PowerShell) type the .\ in front:
rem   .\flash.cmd                       list ports
rem   .\flash.cmd COM5                  new bootloader
rem   .\flash.cmd COM5 oldbootloader    clone boards that fail to sync
setlocal
rem Some Windows profiles have a broken/redirected Documents known-folder; pin
rem the sketchbook dir so arduino-cli never fails with "cannot get documents".
set "ARDUINO_DIRECTORIES_USER=%LOCALAPPDATA%\Arduino15\user"
where arduino-cli >nul 2>nul || set "PATH=%PATH%;%LOCALAPPDATA%\Programs\arduino-cli"
where arduino-cli >nul 2>nul || (echo arduino-cli not found - run SETUP-FRIEND.cmd first or install from arduino.cc & exit /b 1)
if "%~1"=="" (
  arduino-cli board list
  echo.
  echo Usage: .\flash.cmd COM5 [oldbootloader]
  exit /b 0
)
set FQBN=arduino:avr:nano
if /i "%~2"=="oldbootloader" set FQBN=arduino:avr:nano:cpu=atmega328old
cd /d "%~dp0"
arduino-cli compile --fqbn %FQBN% --build-property "compiler.cpp.extra_flags=-DON_BOARD" --output-dir build-board sketch\hovercraft_ta1 || exit /b 1
arduino-cli upload -p %~1 --fqbn %FQBN% --input-dir build-board sketch\hovercraft_ta1 || exit /b 1
echo Flashed. Serial monitor at 9600 baud - press Ctrl+C to exit.
arduino-cli monitor -p %~1 --config baudrate=9600
