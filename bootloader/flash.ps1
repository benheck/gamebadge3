# flash.ps1 — Flash gameBadge combined firmware to a Pico in BOOTSEL mode.
# Copies UF2 to the RPI-RP2 drive. No extra tools or drivers needed.
#
# Usage: .\flash.ps1 [-UF2 path\to\file.uf2]

param(
    [string]$UF2
)

$ErrorActionPreference = "Stop"

if (-not $UF2) { $UF2 = Join-Path $PSScriptRoot "gamebadge_combined.uf2" }
if (-not (Test-Path $UF2)) {
    Write-Host "ERROR: UF2 not found: $UF2" -ForegroundColor Red
    exit 1
}

Write-Host "Flashing $(Split-Path -Leaf $UF2)..." -ForegroundColor Cyan

# Wait for RPI-RP2 drive
Write-Host -NoNewline "Waiting for device in BOOTSEL mode"
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
            Write-Host "`nNo device found. Hold BOOTSEL while plugging in USB, then re-run." -ForegroundColor Red
            exit 1
        }
    }
}
Write-Host " found at $drive"

Write-Host "Copying UF2..."
Copy-Item $UF2 -Destination $drive -Force

Write-Host -NoNewline "Waiting for reboot"
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 500
    Write-Host -NoNewline "."
    if (-not (Get-Volume -FileSystemLabel "RPI-RP2" -ErrorAction SilentlyContinue)) { break }
}
Write-Host ""
Write-Host "Done!" -ForegroundColor Green
