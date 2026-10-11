# Creates the self-contained Linux package (Qt bundled) in deploy-linux\ via Docker:
#   deploy-linux\oborcopy-<version>-linux-x86_64\   (ready-to-run folder)
#   deploy-linux\oborcopy-<version>-linux-x86_64.tar.gz
# Usage: .\scripts\deploy-linux.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

Push-Location $root
try {
    if (Test-Path deploy-linux) { Remove-Item -Recurse -Force deploy-linux }
    # docker prints progress on stderr; Windows PowerShell must not treat that as an error.
    $ErrorActionPreference = "Continue"
    docker build -f docker/Dockerfile.linux --target deploy --output type=local,dest=deploy-linux . 2>&1 |
        ForEach-Object { "$_" }
    $ErrorActionPreference = "Stop"
    if ($LASTEXITCODE -ne 0) { throw "Linux build, tests or bundling failed" }
    Write-Host "Package: $root\deploy-linux"
}
finally {
    Pop-Location
}
