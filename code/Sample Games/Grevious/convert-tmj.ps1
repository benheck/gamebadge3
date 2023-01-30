$script = $MyInvocation.MyCommand.Source
$folder = Split-Path $script -Parent
$tilesFile = "$folder\level_tiles.txt"
$jsoncontent = Get-Content $folder\testlevel.tmj | ConvertFrom-Json
$outputFile = "$folder\level01.txt"

$layers = $jsoncontent.layers
$properties = "hp", "points", "speedx", "speedy", "state", "path"

# Remove any pre-existing output files
if (Test-Path $outputFile) { Remove-Item $outputFile -Force }

$mobData = ""

foreach ($layer in $layers) {
    if ($layer.Name -ne "Background" -And $layer.Visible)
    {
        $moblayer = $layer
        $mobs = $moblayer.objects | Sort-Object -Property x
        
        foreach ($mob in $mobs)
        {
            if ($mob.type -ne "View" -And $mob.x -ne 0 )
            {
                $outputString = "{ $($mob.class), $($mob.x), $($mob.y), "

                for ($i = 0; $i -lt $properties.length; $i++) {
                    foreach ($mobProperty in $mob.properties) {
                        if ($mobProperty.Name -eq $properties[$i]) {
                            $outputString += $mobProperty.value
                        }
                    }
                    if ($i -lt $properties.Length - 1) {
                        $outputString += ", "
                    }
                }

                $outputString += " }"
                $mobData += $outputString + ", `r`n"
            }
        }
    }
}
if ($mobData.EndsWith("`r`n")) {
    $mobData = $mobData.Remove($mobData.Length - 3)
}

if ($mobData.EndsWith(", ")) {
    $mobData = $mobData.Remove($mobData.Length - 2)
}

"{" + $mobData + "};" | Out-File $outputFile -Append

$tileLineEndString = " },`r`n{ "
$tileEndString = ",`r`n{ "
$tileDataString = "{ "
foreach ($layer in $layers)
{
    if ($layer.type -eq "tilelayer")
    {
        $numTiles = $layer.data.count
        $tilesPerRow = $numTiles / 16
        $tileCtr = 0
        foreach ($tileData in $layer.data) {
            $tileData -= 1
            $tileDataString = $tileDataString + $tileData
            $tileCtr += 1
            if ($tileCtr -eq $tilesPerRow) {
                $tileDataString += $tileLineEndString
                $tileCtr = 0
            } else {
                $tileDataString = $tileDataString + ","
            }
        }
    }
}
if ($tileDataString.EndsWith($tileEndString)) {
    $tileDataString = $tileDataString.Remove($tileDataString.Length - $tileEndString.Length)
}
$tileDataString | Out-File $tilesFile