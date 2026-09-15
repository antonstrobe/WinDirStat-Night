param([Parameter(Mandatory)][string]$ExePath)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$runDir = Join-Path $repo ('build\night-smoke\' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $runDir 'sample'
New-Item -ItemType Directory -Path (Join-Path $fixture 'documents') -Force | Out-Null
$testExe = Join-Path $runDir 'WinDirStat.exe'
Copy-Item -LiteralPath $ExePath -Destination $testExe
# An isolated profile prevents the test from changing the user's settings.
[IO.File]::WriteAllText((Join-Path $runDir 'WinDirStat.ini'), "[Options]`r`nDarkMode=1`r`nLanguageId=1033`r`nAutoElevate=0`r`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllBytes((Join-Path $fixture 'sample.bin'), [byte[]]::new(1048576))
[IO.File]::WriteAllText((Join-Path $fixture 'documents\пример.txt'), 'Проверка русских имён файлов.', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllBytes((Join-Path $fixture 'documents\empty.txt'), [byte[]]::new(0))
$expected = @(Get-ChildItem -LiteralPath $fixture -Recurse -File)
$csv = Join-Path $runDir 'scan.csv'
$process = Start-Process -FilePath $testExe -ArgumentList @('"' + $fixture + '"', '/saveto', '"' + $csv + '"') -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(45000)) {
    Stop-Process -Id $process.Id -ErrorAction SilentlyContinue
    throw 'The fixture scan did not finish within 45 seconds.'
}
$process.Refresh()
if ($process.ExitCode -ne 0) { throw "Scan failed with exit code $($process.ExitCode)." }
$rows = @(Import-Csv -LiteralPath $csv -Encoding UTF8)
if ($rows.Count -ne 5) { throw "Expected 5 rows (3 files and 2 directories), got $($rows.Count)." }
# CSV column order is stable; labels depend on the selected UI language.
$root = @($rows[0].PSObject.Properties.Value)
$total = ($expected | Measure-Object -Property Length -Sum).Sum
if ($root[0] -ne $fixture -or [long]$root[1] -ne 3 -or [long]$root[2] -ne 1 -or [long]$root[3] -ne $total) {
    throw 'Exported root path, counts, or logical size differ from the filesystem.'
}
foreach ($file in $expected) {
    $found = @($rows | Where-Object { @($_.PSObject.Properties.Value)[0] -eq $file.FullName })
    if ($found.Count -ne 1) { throw "File missing or duplicated in export: $($file.Name)" }
    $values = @($found[0].PSObject.Properties.Value)
    if ([long]$values[3] -ne $file.Length) { throw "Incorrect logical size: $($file.Name)" }
}
Write-Output "PASS: 3 files, 1 subdirectory, $total bytes; Unicode and empty file verified."
Write-Output "Evidence: $csv"