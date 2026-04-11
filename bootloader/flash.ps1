# flash.ps1 — Flash gameBadge combined firmware to a Pico via picotool.
# Usage: .\flash.ps1 [-UF2 path\to\file.uf2]

param(
    [string]$UF2
)

$ErrorActionPreference = "Stop"

# Find picotool
function Find-Picotool {
    $candidate = Get-Command picotool -ErrorAction SilentlyContinue
    if ($candidate) { return $candidate.Source }
    foreach ($pattern in @(
        ".\picotool.exe",
        "$env:USERPROFILE\.pico-sdk\picotool\*\picotool.exe",
        "$env:LOCALAPPDATA\Programs\Raspberry Pi\Pico SDK*\picotool\*\picotool.exe",
        "C:\Program Files\Raspberry Pi\Pico SDK*\picotool\*\picotool.exe"
    )) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    return $null
}

$picotool = Find-Picotool
if (-not $picotool) {
    Write-Host "ERROR: picotool not found. Install it or place picotool.exe in this directory." -ForegroundColor Red
    Write-Host "Download: https://github.com/raspberrypi/picotool/releases"
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if (-not $UF2) { $UF2 = Join-Path $scriptDir "build\gamebadge_combined.uf2" }
if (-not (Test-Path $UF2)) {
    Write-Host "ERROR: UF2 not found: $UF2" -ForegroundColor Red
    exit 1
}

Write-Host "Flashing $(Split-Path -Leaf $UF2)..." -ForegroundColor Cyan

# Wait for device
Write-Host -NoNewline "Waiting for device in BOOTSEL mode"
$tries = 0
while ($true) {
    & $picotool info 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { break }
    Start-Sleep -Milliseconds 500
    Write-Host -NoNewline "."
    $tries++
    if ($tries -ge 20) {
        Write-Host "`nNo device found. Plug in Pico in BOOTSEL mode and re-run." -ForegroundColor Red
        exit 1
    }
}
Write-Host " found!"

& $picotool load $UF2
if ($LASTEXITCODE -ne 0) { Write-Host "Flash failed!" -ForegroundColor Red; exit 1 }

& $picotool verify $UF2 2>&1 | Select-Object -Last 2
if ($LASTEXITCODE -ne 0) { Write-Host "Verify failed!" -ForegroundColor Red; exit 1 }

& $picotool reboot 2>&1 | Out-Null
Write-Host "Done! Device rebooted." -ForegroundColor Green
