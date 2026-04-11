# flash_batch.ps1 — Flash gameBadge combined UF2 to multiple Picos in sequence.
# Plug each Pico in BOOTSEL mode, press Enter to flash, repeat.
#
# Usage: .\flash_batch.ps1 [-UF2 path\to\combined.uf2]

param(
    [string]$UF2
)

$ErrorActionPreference = "Stop"

# Find picotool on PATH, common install locations, or local directory
function Find-Picotool {
    $candidate = Get-Command picotool -ErrorAction SilentlyContinue
    if ($candidate) { return $candidate.Source }

    $searchPaths = @(
        ".\picotool.exe",
        "$env:USERPROFILE\.pico-sdk\picotool\*\picotool.exe",
        "$env:LOCALAPPDATA\Programs\Raspberry Pi\Pico SDK*\picotool\*\picotool.exe",
        "C:\Program Files\Raspberry Pi\Pico SDK*\picotool\*\picotool.exe",
        "$env:PICO_SDK_PATH\..\picotool\picotool.exe"
    )

    foreach ($pattern in $searchPaths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { return $found.FullName }
    }

    return $null
}

$picotool = Find-Picotool
if (-not $picotool) {
    Write-Host "ERROR: picotool not found." -ForegroundColor Red
    Write-Host "Install it or place picotool.exe in this directory."
    Write-Host "Download: https://github.com/raspberrypi/picotool/releases"
    exit 1
}
Write-Host "Using picotool: $picotool" -ForegroundColor DarkGray

# Resolve UF2 path
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if (-not $UF2) {
    $UF2 = Join-Path $scriptDir "build\gamebadge_combined.uf2"
}
if (-not (Test-Path $UF2)) {
    Write-Host "ERROR: UF2 not found: $UF2" -ForegroundColor Red
    Write-Host "Usage: .\flash_batch.ps1 [-UF2 path\to\combined.uf2]"
    exit 1
}

$uf2Size = "{0:N1} KB" -f ((Get-Item $UF2).Length / 1KB)

Write-Host ""
Write-Host "=== gameBadge Batch Flasher ===" -ForegroundColor Cyan
Write-Host "  UF2:  $(Split-Path -Leaf $UF2)"
Write-Host "  Size: $uf2Size"
Write-Host "===============================" -ForegroundColor Cyan
Write-Host ""

$count = 0

while ($true) {
    Write-Host "--------------------------------------------" -ForegroundColor DarkGray
    $unit = $count + 1
    Write-Host "Plug in Pico #$unit in BOOTSEL mode, then press Enter." -ForegroundColor Yellow
    Write-Host "(Type 'q' to quit)"
    $input = Read-Host
    if ($input -eq 'q') { break }

    # Wait for device
    Write-Host -NoNewline "Waiting for device"
    $tries = 0
    $found = $false
    while (-not $found) {
        $result = & $picotool info 2>&1
        if ($LASTEXITCODE -eq 0) {
            $found = $true
        } else {
            Start-Sleep -Milliseconds 500
            Write-Host -NoNewline "."
            $tries++
            if ($tries -ge 20) {
                Write-Host ""
                Write-Host "  No device found after 10s. Check connection and BOOTSEL mode." -ForegroundColor Red
                Write-Host "  Press Enter to retry, or 'q' to quit."
                $input = Read-Host
                if ($input -eq 'q') { break }
                $tries = 0
                Write-Host -NoNewline "Waiting for device"
            }
        }
    }
    if (-not $found) { break }
    Write-Host " found!"

    # Flash
    Write-Host "Flashing..." -ForegroundColor Cyan
    & $picotool load $UF2
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FLASH FAILED - check connection and retry." -ForegroundColor Red
        continue
    }

    # Verify
    Write-Host "Verifying..." -ForegroundColor Cyan
    $verifyOutput = & $picotool verify $UF2 2>&1
    $verifyOutput | Select-Object -Last 4 | Write-Host
    if ($verifyOutput -match "ERROR") {
        Write-Host "VERIFY FAILED - do not use this unit. Reflash." -ForegroundColor Red
        continue
    }

    # Reboot
    $count++
    Write-Host "Rebooting..."
    & $picotool reboot 2>&1 | Out-Null
    Write-Host "Pico #$count done!" -ForegroundColor Green
    Write-Host ""
}

Write-Host ""
Write-Host "Done. Flashed $count device(s)." -ForegroundColor Cyan
