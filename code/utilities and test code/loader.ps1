$currentScriptPath = Split-Path -Parent -Path $MyInvocation.MyCommand.Path

$programs = @(
    @("Badgeris", "$currentScriptPath\..\Sample Games\badgeris\badgeris.uf2", "$currentScriptPath\..\Sample Games\badgeris\badgeris", "badgeris\"),
    @("Badgeris 3B", "$currentScriptPath\..\Sample Games\badgeris\badgeris-3b.uf2", "$currentScriptPath\..\Sample Games\badgeris\badgeris", "badgeris\"),
    @("Catskillvania", "$currentScriptPath\..\Sample Games\Catskill\catskill.uf2", "$currentScriptPath\..\Sample Games\Catskill Game Assets\_flash MSD contents", ""),
    @("Catskillvania 3B", "$currentScriptPath\..\Sample Games\Catskill\catskill-3b.uf2", "$currentScriptPath\..\Sample Games\Catskill Game Assets\_flash MSD contents", ""),
    @("NES Jukebox", "$currentScriptPath\..\Sample Games\NESJukebox\nesjukebox.uf2", "$currentScriptPath\..\Sample Games\NESJukebox\nesjukebox", "nesjukebox\"),
    @("NES Jukebox 3B", "$currentScriptPath\..\Sample Games\NESJukebox\nesjukebox-3b.uf2", "$currentScriptPath\..\Sample Games\NESJukebox\nesjukebox", "nesjukebox\"),
    @("Hardware Tester", "$currentScriptPath\..\Sample Games\HardwareTester\HardwareTester.uf2", $null, $null),
    @("Emulator - C64", "$currentScriptPath\..\emulators\pico64.uf2", $null, $null),
    @("Emulator - Atari VCS", "$currentScriptPath\..\emulators\picovcs.uf2", $null, $null)
)

function Check-USBDrivePresence {
    param (
        [string]$volumeName
    )

    $usbDrive = Get-WmiObject -Query "SELECT * FROM Win32_Volume WHERE Label = '$volumeName' AND DriveType = 2"
    
    if ($usbDrive) {
        return $usbDrive.DriveLetter
    } else {
        return $null
    }
}

function FinishUp {
    $colors = "Magenta", "Yellow", "Cyan", "Green", "Red", "Blue"
    $message = "All done. Disconnect and reboot the gameBadge to enjoy your program!"

    foreach ($char in $message.ToCharArray()) {
        $color = $colors[(Get-Random -Minimum 0 -Maximum $colors.Count)]
        Write-Host $char -ForegroundColor $color -NoNewline
    }

    Write-Host
}

function InfoMessage {
    param (
        [string]$message
    )

    Write-Host "INFO: $message" -ForegroundColor Green
}

function WarnMessage {
    param (
        [string]$message
    )

    Write-Host "WARN: $message" -ForegroundColor Yellow
}

function main {
    $driveLetter = Check-USBDrivePresence "RPI-RP2"
    if (-not $driveLetter) {
        WarnMessage "gameBadge not found. Please boot into UF2 mode and connect the gameBadge. Waiting for connection..."
    }
    while (-not $driveLetter) {
        Start-Sleep -Seconds 1
        $driveLetter = Check-USBDrivePresence "RPI-RP2"
    }

    InfoMessage "gameBadge found in UF2 Mode. Drive letter: $driveLetter"

    ##
    ## Display list of programs and ask the user to select one
    ##
    InfoMessage "Available programs:"
    for ($i = 0; $i -lt $programs.Count; $i++) {
        $program = $programs[$i]
        Write-Host "$($i + 1). " -NoNewline -ForegroundColor Green
        Write-Host "$($program[0])" -ForegroundColor Cyan
    }

    $selection = Read-Host "Enter the number of the program you want to load"

    ## 
    ## Exit the script if a user doesn't select a valid option
    ##
    if (-not $selection -or $selection -lt 1 -or $selection -gt $programs.Count) {
        InfoMessage "Invalid selection. Ending script."
        return
    }

    ##
    ## Confirm the user's selection
    ##
    $selectedProgram = $programs[$selection - 1]
    InfoMessage "Selected program: $($selectedProgram[0])"


    ## 
    ## Format the flash drive
    ##
    InfoMessage "Formatting gameBadge..."
    $flashFormatFilePath = "$currentScriptPath\flash_format.uf2"
    $destinationPath = "$driveLetter\flash_format.uf2"
    Copy-Item -Path $flashFormatFilePath -Destination $destinationPath -Force
    InfoMessage "gameBadge formatted. Waiting for reconnection..."

    ##
    ## Wait for the USB drive to reappear after the flash program runs
    ##
    Start-Sleep -Seconds 5
    $usbDrive = Check-USBDrivePresence "RPI-RP2"
    while (-not $usbDrive) {
        Start-Sleep -Seconds 1
        $usbDrive = Check-USBDrivePresence "RPI-RP2"
    }
    InfoMessage "gameBadge Reconnected"

    ##
    ## Copy the selected program UF2 to the gameBadge
    ##
    $programName = $selectedProgram[0]
    $programUf2Path = $selectedProgram[1]
    $destinationProgramPath = "$driveLetter\$programName.uf2"
    Copy-Item -Path $programUf2Path -Destination $destinationProgramPath -Force
    InfoMessage "$programName copied to the gameBadge. Waiting for reconnection..."

    ##
    ## Check if there are program assets to copy
    ##
    $programFolderPath = $selectedProgram[2]
    if (-not $programFolderPath) {
        FinishUp
        return
    }

    ##
    ## Wait for the gameBadge to reconnect as a flash drive
    ##
    $flashDrive = Check-USBDrivePresence "EXT FLASH"
    while (-not $flashDrive) {
        Start-Sleep -Seconds 1
        $flashDrive = Check-USBDrivePresence "EXT FLASH"
    }
    InfoMessage "gameBadge reconnected."
    
    ##
    ## Copy the folder of the selected program to the gameBadge
    ##
    $programFolderPath = $selectedProgram[2]
    $programFolderDest = $selectedProgram[3]
    if ($programFolderPath) {
        $destinationFolderPath = "$flashDrive\$programFolderDest"
        if (-not [string]::IsNullOrEmpty($programFolderDest)) {
            New-Item -ItemType Directory -Path $destinationFolderPath -Force | Out-Null
        }

        Write-Host "Copying $programName assets to gameBadge..." -NoNewline -ForegroundColor Magenta
        $files = Get-ChildItem -Path $programFolderPath

        foreach ($file in $files) {
            $counter++
            Write-Host "." -NoNewline -ForegroundColor Magenta
            Copy-Item -Path $file.FullName -Destination $destinationFolderPath -Force -Recurse
        }

        Write-Host ""
        InfoMessage "$programName assets copied to gameBadge."
    }    

    ##
    ## All done - display final message
    ##
    FinishUp
}

main
