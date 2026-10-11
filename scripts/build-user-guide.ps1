# Builds docs\user-guide\Oborcopy-User-Guide.pdf from the Markdown source.
#   1. Pandoc (Docker image) converts Markdown -> self-contained HTML (images embedded, guide.css).
#   2. Microsoft Edge (headless) prints the HTML to PDF.
# Usage: .\scripts\build-user-guide.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$guide = "$root\docs\user-guide"
$html = "$guide\Oborcopy-User-Guide.html"
$pdf = "$guide\Oborcopy-User-Guide.pdf"
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

$ErrorActionPreference = "Continue"   # docker writes progress to stderr
docker run --rm -v "${root}:/data" -w /data/docs/user-guide pandoc/core:3.5 `
    Oborcopy-User-Guide.md -o Oborcopy-User-Guide.html `
    --standalone --embed-resources --css guide.css `
    --metadata pagetitle="Oborcopy User Guide" --from gfm+raw_html 2>&1 | ForEach-Object { "$_" }
$ErrorActionPreference = "Stop"
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $html)) { throw "Pandoc conversion failed" }

$profileDir = Join-Path $env:TEMP "oborcopy-edge-pdf"
$p = Start-Process -FilePath $edge -Wait -PassThru -ArgumentList @(
    "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--user-data-dir=`"$profileDir`"",
    "--print-to-pdf=`"$pdf`"", "`"file:///$($html -replace '\\','/')`"")
if (-not (Test-Path $pdf)) { throw "PDF printing failed (exit $($p.ExitCode))" }
Remove-Item $html
Write-Host "User guide: $pdf ($([math]::Round((Get-Item $pdf).Length / 1MB, 1)) MB)"
