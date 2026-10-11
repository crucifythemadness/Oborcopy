# Builds Oborcopy with the Qt MinGW kit and runs the tests.
# Usage: .\scripts\build-windows.ps1 [-Preset windows-mingw|windows-mingw-debug] [-SkipTests]
param(
    [string]$Preset = "windows-mingw",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cmakeBin = "C:\Qt\Tools\CMake_64\bin"
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\Ninja;$cmakeBin;$env:PATH"

Push-Location $root
try {
    & "$cmakeBin\cmake.exe" --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

    & "$cmakeBin\cmake.exe" --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }

    if (-not $SkipTests) {
        $env:QT_QPA_PLATFORM = "offscreen"
        & "$cmakeBin\ctest.exe" --test-dir "build\$Preset" --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
    }
}
finally {
    Pop-Location
}
