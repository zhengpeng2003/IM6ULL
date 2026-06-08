$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$Source = Join-Path $Root "Linux_data"
$Target = "debian@192.168.10.50:/home/debian/Linux_data"

if (-not (Test-Path $Source)) {
    Write-Host "Source not found: $Source"
    exit 1
}

Write-Host "Sync Linux_data to board..."
Write-Host "From: $Source"
Write-Host "To:   $Target"

Push-Location $Source
scp -r . $Target
Pop-Location

Write-Host "Done."
