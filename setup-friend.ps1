# ENGR290 one-time laptop setup for teammates (Windows 10/11, no admin needed except maybe git).
# Run it by double-clicking SETUP-FRIEND.cmd in the project folder, or:
#   powershell -NoProfile -ExecutionPolicy Bypass -File setup-friend.ps1
# Optional: -NoGit   (skip installing git)   -ProjectsDir "D:\somewhere"
param(
  [string]$ProjectsDir = "$env:USERPROFILE\Desktop",
  [switch]$NoGit
)
$ErrorActionPreference = 'Stop'
function Step($m) { Write-Host "" ; Write-Host "== $m ==" -ForegroundColor Cyan }

# ---- 1. VS Code ----
Step "1/7 VS Code (editor)"
$codeCmd = "$env:LOCALAPPDATA\Programs\Microsoft VS Code\bin\code.cmd"
if (Test-Path $codeCmd) {
  Write-Host "already installed - skipping"
} else {
  winget install -e --id Microsoft.VisualStudioCode --accept-package-agreements --accept-source-agreements --disable-interactivity --silent
  if (-not (Test-Path $codeCmd)) { throw "VS Code install failed (no code.cmd found)" }
  Write-Host "installed"
}

# ---- 2. arduino-cli (compiler + uploader) ----
Step "2/7 arduino-cli (compiles the code)"
$acli = "$env:LOCALAPPDATA\Programs\arduino-cli\arduino-cli.exe"
if (Test-Path $acli) {
  Write-Host "already installed - skipping"
} else {
  $zip = "$env:TEMP\arduino-cli.zip"
  Invoke-WebRequest -Uri 'https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip' -OutFile $zip
  New-Item -ItemType Directory -Force -Path "$env:LOCALAPPDATA\Programs\arduino-cli" | Out-Null
  Expand-Archive -Force $zip "$env:LOCALAPPDATA\Programs\arduino-cli"
  $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
  if ($userPath -notlike '*arduino-cli*') {
    [Environment]::SetEnvironmentVariable('Path', "$userPath;$env:LOCALAPPDATA\Programs\arduino-cli", 'User')
  }
  Write-Host "installed"
}
$env:Path += ";$env:LOCALAPPDATA\Programs\arduino-cli"   # use it right away

# ---- 3. AVR chip support ----
Step "3/7 Arduino AVR support (chip definitions)"
$cores = (& $acli core list) | Out-String
if ($cores -like '*arduino:avr*') {
  Write-Host "already installed - skipping"
} else {
  & $acli core update-index
  & $acli core install arduino:avr
  Write-Host "installed"
}

# ---- 4. Wokwi simulator extension ----
Step "4/7 Wokwi extension (the emulator inside VS Code)"
$extList = (& $codeCmd --list-extensions) | Out-String
if ($extList -like '*wokwi*') {
  Write-Host "already installed - skipping"
} else {
  & $codeCmd --install-extension wokwi.wokwi-vscode | Out-Null
  Write-Host "installed wokwi.wokwi-vscode"
}

# ---- 5. git (optional but recommended for group work) ----
if (-not $NoGit) {
  Step "5/7 git (to share code on GitHub)"
  if (Get-Command git -ErrorAction SilentlyContinue) {
    Write-Host "already installed - skipping"
  } else {
    try {
      winget install -e --id Git.Git --accept-package-agreements --accept-source-agreements --disable-interactivity --silent
      Write-Host "installed (a UAC prompt may have appeared)"
    } catch {
      Write-Warning "git install failed - you can still simulate; for GitHub, download ZIPs or install git later"
    }
  }
} else {
  Step "5/7 git - skipped (-NoGit)"
}

# ---- 6. project folder ----
Step "6/7 Project folder"
$dest = Join-Path $ProjectsDir "HoverCraft_Class_Arduino"
if (Test-Path $dest) {
  Write-Host "already exists - using it: $dest"
} else {
  $z = "$env:TEMP\hovercraft_repo.zip"
  Invoke-WebRequest -Uri 'https://github.com/dchirigiu/HoverCraft_Class_Arduino/archive/refs/heads/main.zip' -OutFile $z
  $tmp = "$env:TEMP\hovercraft_extract"
  if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
  Expand-Archive -Force $z $tmp
  $inner = (Get-ChildItem $tmp -Directory | Select-Object -First 1).FullName
  Copy-Item $inner $dest -Recurse
  Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host "downloaded to: $dest"
}
Set-Location $dest

# ---- 7. first compile ----
Step "7/7 First compile (needs to finish before the simulator works)"
& $acli compile -b arduino:avr:nano --output-dir build sketch/hovercraft_ta1
if ($LASTEXITCODE -ne 0) { throw "compile failed" }

Write-Host ""
Write-Host "ALL DONE" -ForegroundColor Green
Write-Host @"

Next steps:
  1. Open the folder in VS Code:  code "$dest"
     (or: right-click the folder > Open with Code)
  2. Press F1, type "wokwi", choose "Wokwi: Start Simulator"
  3. Click the potentiometer (IR stand-in) or the HC-SR04 to change distances,
     and watch the SERIAL MONITOR tab print distances.
Note: if VS Code was JUST installed, close and reopen it once so it sees the tools.
"@
