[CmdletBinding()]
param(
    [Parameter(Mandatory=$true, ValueFromPipeline=$true)]
    [string[]]$FileNames
)

function ConvertTo-FileName {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$InputString
    )
    $regex = '[^\w\-\.]'
    $output = $InputString -replace $regex, '_'
    return $output
}

function Convert-HexToDecimal {
    param(
        [char]$hex
    )
    [int]$decimal = [Convert]::ToInt32($hex, 16)
    return $decimal
}


$notes = @("---", "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2", "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3", "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4", "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5", "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6", "C-7", "C#7", "D-7")

$script = $MyInvocation.MyCommand.Source
$folder = Split-Path $script -Parent

foreach ($file in $FileNames) 
{
    $file_info = Get-ChildItem $file
    $file_contents = Get-Content $file
    $basename = ConvertTo-FileName $file_info.BaseName
    $nameUpper = $basename.ToUpper()
    $outputFile = "$folder\$basename.h"

    $cr = "`r`n"
    $predata =  "#ifndef $($nameUpper)_H$cr" +
                "#define $($nameUpper)_H$cr$cr" + 
                "#pragma once$cr"

    $postdata = "#endif"

    # Remove any pre-existing output files
    if (Test-Path $outputFile) { Remove-Item $outputFile -Force }

    # Write headers and directives to the output file
    $predata | Out-File $outputFile -Encoding ascii

    for ($i = 1; $i -lt 5; $i++) 
    {
        $file_contents = Get-Content $file
        $converted_string = "uint16_t $basename$i[] = {"
        $previous_note = ""
        $vol_note = ""
        $previous_volume = "0"

        foreach ($row in $file_contents)
        {
            if (!$row.StartsWith("ROW ")) { continue }

            $row = $row.Trim()
            $cols = $row.Split("\s:\s")
            $token = $cols[$i].Trim()
            
            # $token = "F#3 00 0 P7F"
            $tokensplit =   $token.Split(" ")
            $note =         $tokensplit[0]
            $instrument =   $tokensplit[1]
            $volume =       $tokensplit[2]
            $effect =       $tokensplit[3]

            if ($volume -ne ".") {
                $volDec = Convert-HexToDecimal $volume
                $previous_volume = $volDec
            } else {
                $volDec = $previous_volume
            }
            
            # If the note ends with a #, it's in the noise channel and the only acceptable values are 0 - F
            if ($note.EndsWith("#")) {
                $noteVal = Convert-HexToDecimal $note[0]
                $stringVal = ($noteVal -shl 4) -bxor $volDec
                $converted_string += $stringVal

                $previous_note = $noteVal
            } 
            elseif ($note -ne "...") 
            {
                $noteIndex = $notes.IndexOf($note)
                $stringVal = ($noteIndex -shl 4) -bxor $volDec
                $converted_string += $stringVal

                $previous_note = $notes.IndexOf($note)
            } else 
            {
                $converted_string += ($previous_note -shl 4) -bxor $volDec
            }
            $converted_string += ", "
        }

        if ($converted_string.EndsWith(", ")) {
            $converted_string = $converted_string.Remove($converted_string.Length - 2)
            $converted_string += "};"
        }

        # Output the audio data to the file
        $converted_string | Out-File $outputFile -Append -Encoding ascii
    }

    # Write remaining directives to the output file
    $postdata | Out-File $outputFile -Append -Encoding ascii
}