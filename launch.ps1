# Dreadnought Revival Launcher (PowerShell)
# Reverse-engineered from DreadGame-Win64-Shipping.exe:
#   GatewayAddress= and GatewayPort= are parsed by the game's WebServicesPlugin
#   via UE4's FParse::Value() to locate the HTTP gateway server.
#   No DLL injection is required for the REST API layer.

param(
    [string]$GatewayHost = "127.0.0.1",
    [int]   $GatewayPort = 8080,
    [string]$GamePath    = "$env:ProgramFiles(x86)\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64"
)

$exe = Join-Path $GamePath "DreadGame-Win64-Shipping.exe"

if (-not (Test-Path $exe)) {
    Write-Error "Game executable not found: $exe`nSet -GamePath to your Dreadnought installation."
    exit 1
}

Write-Host "Starting Dreadnought with Revival Server at ${GatewayHost}:${GatewayPort}" -ForegroundColor Cyan

# GatewayAddress= and GatewayPort= are read by the game's WebServicesPlugin (confirmed via binary analysis).
# -log enables the UE4 log file in %LOCALAPPDATA%\DreadGame\Saved\Logs\
$args = @(
    "-GatewayAddress=$GatewayHost",
    "-GatewayPort=$GatewayPort",
    "-log"
)

Start-Process -FilePath $exe -ArgumentList $args
