# flash_batch.ps1 — Flash gameBadge combined UF2 to multiple Picos in sequence.
# Copies UF2 to the RPI-RP2 drive. No extra tools or drivers needed.
# Plug each Pico in BOOTSEL mode, press Enter to flash, repeat.
#
# Usage: .\flash_batch.ps1 [-UF2 path\to\combined.uf2]

param(
    [string]$UF2
)

$ErrorActionPreference = "Stop"

if (-not $UF2) {
    $UF2 = Join-Path $PSScriptRoot "gamebadge_combined.uf2"
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

    # Wait for RPI-RP2 drive
    Write-Host -NoNewline "Waiting for device"
    $tries = 0
    $drive = $null
    while (-not $drive) {
        $drive = Get-Volume -FileSystemLabel "RPI-RP2" -ErrorAction SilentlyContinue |
                 ForEach-Object { "$($_.DriveLetter):\" }
        if (-not $drive) {
            Start-Sleep -Milliseconds 500
            Write-Host -NoNewline "."
            $tries++
            if ($tries -ge 20) {
                Write-Host ""
                Write-Host "  No device found after 10s. Hold BOOTSEL while plugging in USB." -ForegroundColor Red
                Write-Host "  Press Enter to retry, or 'q' to quit."
                $input = Read-Host
                if ($input -eq 'q') { break }
                $tries = 0
                Write-Host -NoNewline "Waiting for device"
            }
        }
    }
    if (-not $drive) { break }
    Write-Host " found at $drive"

    # Flash
    Write-Host "Copying UF2..." -ForegroundColor Cyan
    Copy-Item $UF2 -Destination $drive -Force

    # Wait for device to disconnect (it reboots after receiving UF2)
    Write-Host -NoNewline "Waiting for reboot"
    $rebooted = $false
    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Milliseconds 500
        Write-Host -NoNewline "."
        if (-not (Get-Volume -FileSystemLabel "RPI-RP2" -ErrorAction SilentlyContinue)) {
            $rebooted = $true
            break
        }
    }
    Write-Host ""

    $count++
    if ($rebooted) {
        Write-Host "Pico #$count done!" -ForegroundColor Green
    } else {
        Write-Host "Pico #$count flashed (power cycle if it doesn't start)." -ForegroundColor Yellow
    }
    Write-Host ""
}

Write-Host ""
Write-Host "Done. Flashed $count device(s)." -ForegroundColor Cyan
