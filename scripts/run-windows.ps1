# Starts the locally built GUI with the Qt runtime on PATH (no deployment needed).
# Usage: .\scripts\run-windows.ps1 [-Preset windows-mingw]
param([string]$Preset = "windows-mingw")
$root = Split-Path -Parent $PSScriptRoot
$env:PATH = "C:\Qt\6.10.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Start-Process -FilePath "$root\build\$Preset\Oborcopy.exe"
