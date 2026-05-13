# Dreadnought Revival Launcher (PowerShell fallback)
# Use DreadnoughtLauncher.exe (via Steam) as the primary way to start the game.
# This script is a fallback for launching outside of Steam.

param(
    [string]$GatewayHost    = "127.0.0.1",
    [int]   $GatewayPort    = 8080,
    [string]$YFirmamentHost = "",
    [int]   $YFirmamentPort = 9000,
    [string]$Win64Path      = "${env:ProgramFiles(x86)}\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64"
)

if (-not $YFirmamentHost) { $YFirmamentHost = $GatewayHost }

$exe = Join-Path $Win64Path "DreadGame-Win64-Shipping.exe"

if (-not (Test-Path $exe)) {
    Write-Error "Game executable not found: $exe`nSet -Win64Path to your Dreadnought Win64 directory."
    exit 1
}

Write-Host "Starting Dreadnought -> Revival Server at ${GatewayHost}:${GatewayPort}" -ForegroundColor Cyan

Start-Process -FilePath $exe `
    -ArgumentList "-GatewayAddress=$GatewayHost", "-GatewayPort=$GatewayPort", `
                  "-YFirmamentAddress=$YFirmamentHost", "-YFirmamentPort=$YFirmamentPort", `
                  "-log" `
    -WorkingDirectory $Win64Path
