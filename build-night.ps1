param([ValidateSet('x64','Win32','ARM64')][string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio 2022 Build Tools with C++ tools, MFC, and Windows SDK are required. Alternatively run the Night portable workflow on GitHub.'
}
$msbuild = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild with the C++ toolchain was not found.' }
& $msbuild (Join-Path $repo 'windirstat.sln') /m /t:Build /p:Configuration=Release "/p:Platform=$Platform" /p:ExternalCompilerOptions=/DPRODUCTION=0 /nologo
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
$arch = if ($Platform -eq 'Win32') { 'x86' } else { $Platform.ToLowerInvariant() }
$exe = Join-Path $repo "build\WinDirStat_$arch.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "Build output is missing: $exe" }
if ($arch -eq 'x64') {
    & $msbuild (Join-Path $repo 'night\NightTests.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Selection test build failed.' }
    & (Join-Path $repo 'build\night-unit\NightTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Selection regression tests failed.' }
    & (Join-Path $repo 'night\Test-ClassicPalette.ps1')
    foreach ($palette in 0, 1) { & (Join-Path $repo 'night\Test-NightScan.ps1') -ExePath $exe -Palette $palette }
}
$package = Join-Path $repo "publish\WinDirStat-Night-$arch"
New-Item -ItemType Directory -Path $package -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $package 'WinDirStat.exe')
foreach ($file in @('WinDirStat.ini','Start-WinDirStat-Night.cmd')) {
    $destination = Join-Path $package $file
    if (-not (Test-Path -LiteralPath $destination)) {
        Copy-Item -LiteralPath (Join-Path $repo "night\$file") -Destination $destination
    }
}
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE.md'),(Join-Path $repo 'NIGHT-README.md') -Destination $package
Get-FileHash -LiteralPath (Join-Path $package 'WinDirStat.exe') -Algorithm SHA256 | Format-List
Write-Output "Portable build: $package"
