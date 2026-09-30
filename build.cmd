@echo off
rem Compile the sketch on Windows (run before starting the Wokwi simulator).
rem For old-bootloader clone boards compile is identical; only upload differs.
rem ARDUINO_DIRECTORIES_USER pins the sketchbook dir: some Windows profiles have
rem a broken/redirected Documents known-folder, which makes arduino-cli fail
rem with "cannot get documents folder".
setlocal
set "ARDUINO_DIRECTORIES_USER=%LOCALAPPDATA%\Arduino15\user"
where arduino-cli >nul 2>nul || set "PATH=%PATH%;%LOCALAPPDATA%\Programs\arduino-cli"
cd /d "%~dp0"
arduino-cli compile -b arduino:avr:nano --output-dir build sketch\hovercraft_ta1
if errorlevel 1 exit /b 1
echo OK: build\hovercraft_ta1.ino.elf + build\hovercraft_ta1.ino.hex
