$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$header = [IO.File]::ReadAllText((Join-Path $repo 'windirstat\Controls\TreeMap.h'))
$block = [regex]::Match($header, '(?s)OriginalCushionColors\[\]\s*=\s*\{(.*?)\};').Groups[1].Value
$colors = @([regex]::Matches($block, 'RGB\(\s*(\d+),\s*(\d+),\s*(\d+)\)') | ForEach-Object {
    '{0},{1},{2}' -f $_.Groups[1].Value, $_.Groups[2].Value, $_.Groups[3].Value
})
# Golden palette from upstream release/v2.8.0, commit 579dd76d6870b4efbb9497644698155fdab84284.
$original = @('0,0,255','255,0,0','0,255,0','255,255,0','0,255,255','255,0,255',
    '255,170,0','0,85,255','255,0,85','85,255,0','170,0,255','0,255,85',
    '255,0,170','0,170,255','255,85,0','0,255,170','85,0,255','255,255,255')
if (($colors -join ';') -cne ($original -join ';')) { throw 'The classic palette differs from upstream.' }
Write-Output 'PASS: all 18 original colors and their order match WinDirStat 2.8.0.'
