# Dreadnought Revival Launcher (PowerShell)
# Reverse-engineered from DreadGame-Win64-Shipping.exe:
#   GatewayAddress= and GatewayPort= are parsed by the game's WebServicesPlugin
#   via UE4's FParse::Value() to locate the revival HTTP gateway server.
#   Steam must be running — steam_appid.txt (835860) must be in the Win64 working dir.

param(
    [string]$GatewayHost = "127.0.0.1",
    [int]   $GatewayPort = 8080,
    # Path to the Win64 directory containing DreadGame-Win64-Shipping.exe
    [string]$Win64Path   = "${env:ProgramFiles(x86)}\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64"
)

$exe    = Join-Path $Win64Path "DreadGame-Win64-Shipping.exe"
$eacDll = Join-Path $Win64Path "EasyAntiCheat_x64.dll"
$eacBak = Join-Path $Win64Path "EasyAntiCheat_x64.dll.bak"

if (-not (Test-Path $exe)) {
    Write-Error "Game executable not found: $exe`nSet -Win64Path to your Dreadnought Win64 directory."
    exit 1
}

# EAC bypass: rename EasyAntiCheat_x64.dll so the popup is suppressed.
# EAC servers have been offline since 2023.
if ((Test-Path $eacDll) -and -not (Test-Path $eacBak)) {
    Write-Host "[EAC] Disabling EasyAntiCheat (renaming DLL)..." -ForegroundColor Yellow
    try {
        Rename-Item $eacDll "EasyAntiCheat_x64.dll.bak" -ErrorAction Stop
        Write-Host "[EAC] Disabled. Restore with: Rename-Item '$eacBak' EasyAntiCheat_x64.dll" -ForegroundColor Green
    } catch {
        Write-Warning "[EAC] Could not rename EAC DLL: $_. Run as Administrator to suppress the popup."
    }
} elseif (Test-Path $eacBak) {
    Write-Host "[EAC] Already disabled." -ForegroundColor Green
}

# Ensure steam_appid.txt exists so SteamAPI_Init() finds the AppID.
$steamAppIdFile = Join-Path $Win64Path "steam_appid.txt"
if (-not (Test-Path $steamAppIdFile)) {
    "835860" | Set-Content $steamAppIdFile
    Write-Host "[Steam] Created steam_appid.txt with AppID 835860" -ForegroundColor Green
}

# Check if Steam is running.
if (-not (Get-Process -Name "steam" -ErrorAction SilentlyContinue)) {
    Write-Warning "Steam does not appear to be running. Authentication requires Steam to be active."
    $response = Read-Host "Start Steam then press Enter to continue (or Ctrl+C to cancel)"
}

Write-Host "Starting Dreadnought -> Revival Server at ${GatewayHost}:${GatewayPort}" -ForegroundColor Cyan
Write-Host "Log: $env:LOCALAPPDATA\DreadGame\Saved\Logs\DreadGame.log" -ForegroundColor Gray

# Launch with Win64 as the working directory so steam_appid.txt is found by SteamAPI_Init().
# GatewayAddress= and GatewayPort= are read by WebServicesPlugin via FParse::Value().
$launchArgs = @(
    "-GatewayAddress=$GatewayHost",
    "-GatewayPort=$GatewayPort",
    "-log"
)

Start-Process -FilePath $exe -ArgumentList $launchArgs -WorkingDirectory $Win64Path
