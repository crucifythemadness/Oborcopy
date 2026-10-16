# Creates the self-contained Windows package in deploy-win64\ and a ZIP next to it.
# Usage: .\scripts\deploy-windows.ps1 [-SkipBuild]
param([switch]$SkipBuild)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$qtBin = "C:\Qt\6.10.2\mingw_64\bin"
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;$qtBin;$env:PATH"

$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'VERSION (\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
$build = "$root\build\windows-mingw"
$deploy = "$root\deploy-win64"

if (-not $SkipBuild) {
    & "$PSScriptRoot\build-windows.ps1"
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

if (Test-Path $deploy) { Remove-Item -Recurse -Force $deploy }
New-Item -ItemType Directory $deploy | Out-Null
Copy-Item "$build\Oborcopy.exe", "$build\oborcopy-cli.exe", "$build\oborcopy-service.exe" $deploy

foreach ($exe in "Oborcopy.exe", "oborcopy-cli.exe", "oborcopy-service.exe") {
    & "$qtBin\windeployqt.exe" --release --compiler-runtime --no-translations `
        --no-system-d3d-compiler --no-opengl-sw "$deploy\$exe"
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed for $exe" }
}

(Get-Content "$root\packaging\README-windows.txt" -Raw).Replace("@VERSION@", $version) |
    Set-Content -Encoding utf8 "$deploy\README.txt"
Copy-Item -Recurse "$root\packaging\LICENSES" "$deploy\LICENSES"
Copy-Item "$root\docs\user-guide\Oborcopy-User-Guide.pdf" $deploy

$zip = "$root\Oborcopy-$version-win64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path "$deploy\*" -DestinationPath $zip
Write-Host "Package: $deploy"
Write-Host "Archive: $zip"
