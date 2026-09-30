@echo off
rem Double-click helper for setup-friend.ps1 (bypasses script-execution policy).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-friend.ps1" %*
pause
