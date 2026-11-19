# Builds the user guides from their Markdown sources:
#   docs\user-guide\Oborcopy-User-Guide.md -> Oborcopy-User-Guide.pdf (English)
#   docs\user-guide\Benutzerhandbuch.md    -> Benutzerhandbuch.pdf    (German)
#   1. Pandoc (Docker image) converts Markdown -> self-contained HTML (images embedded, guide.css).
#   2. Microsoft Edge (headless) prints the HTML to PDF.
# Usage: .\scripts\build-user-guide.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$guide = "$root\docs\user-guide"
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

$guides = @(
    @{ Name = "Oborcopy-User-Guide"; Title = "Oborcopy User Guide"; Lang = "en" },
    @{ Name = "Benutzerhandbuch"; Title = "Oborcopy Benutzerhandbuch"; Lang = "de" }
)

foreach ($g in $guides) {
    $html = "$guide\$($g.Name).html"
    $pdf = "$guide\$($g.Name).pdf"

    $ErrorActionPreference = "Continue"   # docker writes progress to stderr
    docker run --rm -v "${root}:/data" -w /data/docs/user-guide pandoc/core:3.5 `
        "$($g.Name).md" -o "$($g.Name).html" `
        --standalone --embed-resources --css guide.css `
        --metadata pagetitle="$($g.Title)" --metadata lang=$($g.Lang) --from gfm+raw_html 2>&1 | ForEach-Object { "$_" }
    $ErrorActionPreference = "Stop"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $html)) { throw "Pandoc conversion failed for $($g.Name)" }

    if (Test-Path $pdf) { Remove-Item $pdf }   # so a failed print is not hidden by the old PDF
    $profileDir = Join-Path $env:TEMP "oborcopy-edge-pdf"
    $p = Start-Process -FilePath $edge -Wait -PassThru -ArgumentList @(
        "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--user-data-dir=`"$profileDir`"",
        "--print-to-pdf=`"$pdf`"", "`"file:///$($html -replace '\\','/')`"")
    if (-not (Test-Path $pdf)) { throw "PDF printing failed for $($g.Name) (exit $($p.ExitCode))" }
    Remove-Item $html
    Write-Host "User guide: $pdf ($([math]::Round((Get-Item $pdf).Length / 1MB, 1)) MB)"
}
