# Dreadnought Revival Launcher (PowerShell fallback)
# Use DreadnoughtLauncher.exe (via Steam) as the primary way to start the game.
# This script is a fallback for launching outside of Steam.

param(
    [string]$GatewayHost = "127.0.0.1",
    [int]   $GatewayPort = 8080,
    [string]$Win64Path   = "${env:ProgramFiles(x86)}\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64"
)

$exe    = Join-Path $Win64Path "DreadGame-Win64-Shipping.exe"
$eacDll = Join-Path $Win64Path "EasyAntiCheat_x64.dll"
$eacBak = Join-Path $Win64Path "EasyAntiCheat_x64.dll.bak"

if (-not (Test-Path $exe)) {
    Write-Error "Game executable not found: $exe`nSet -Win64Path to your Dreadnought Win64 directory."
    exit 1
}

# EAC bypass
if ((Test-Path $eacDll) -and -not (Test-Path $eacBak)) {
    Write-Host "[EAC] Disabling EasyAntiCheat..." -ForegroundColor Yellow
    try {
        Rename-Item $eacDll "EasyAntiCheat_x64.dll.bak" -ErrorAction Stop
        Write-Host "[EAC] Done." -ForegroundColor Green
    } catch {
        Write-Warning "[EAC] Could not rename EAC DLL: $_. Run as Administrator."
    }
} elseif (Test-Path $eacBak) {
    Write-Host "[EAC] Already disabled." -ForegroundColor Green
}

Write-Host "Starting Dreadnought -> Revival Server at ${GatewayHost}:${GatewayPort}" -ForegroundColor Cyan

Start-Process -FilePath $exe `
    -ArgumentList "-GatewayAddress=$GatewayHost", "-GatewayPort=$GatewayPort", "-log" `
    -WorkingDirectory $Win64Path
