$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Source = Join-Path $Root "Linux_ui"
$Target = "zhengpeng@192.168.17.139:/work/Qt_Object/qt_hmi"

if (-not (Test-Path $Source)) {
    Write-Host "Source not found: $Source"
    exit 1
}

Write-Host "Sync Linux_ui to Linux build machine..."
Write-Host "From: $Source"
Write-Host "To:   $Target"

Push-Location $Source
scp -r . $Target
Pop-Location

Write-Host "Done."
