# Builds and tests Oborcopy for Linux inside Docker (Ubuntu 24.04, Qt 6.4).
# The Linux binaries are copied to build\linux-out.
# Usage: .\scripts\build-linux.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

Push-Location $root
try {
    docker build -f docker/Dockerfile.linux --target build -t oborcopy-linux-build .
    if ($LASTEXITCODE -ne 0) { throw "Linux build or tests failed" }

    docker build -f docker/Dockerfile.linux --target artifacts --output type=local,dest=build/linux-out .
    if ($LASTEXITCODE -ne 0) { throw "Extracting Linux binaries failed" }
    Write-Host "Linux binaries: $root\build\linux-out"
}
finally {
    Pop-Location
}
