@echo off
:: Dreadnought Revival Launcher (batch fallback)
:: Use DreadnoughtLauncher.exe (via Steam) as the primary way to start the game.
:: This script is a fallback for launching outside of Steam.

setlocal

:: ---- Configure these to match your installation ----
set GATEWAY_HOST=127.0.0.1
set GATEWAY_PORT=8080
set YFIRMAMENT_HOST=%GATEWAY_HOST%
set YFIRMAMENT_PORT=9000
:: Path to the Win64 directory containing DreadGame-Win64-Shipping.exe
set WIN64=C:\Program Files (x86)\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64
:: -----------------------------------------------------

set EXE=%WIN64%\DreadGame-Win64-Shipping.exe

if not exist "%EXE%" (
    echo ERROR: Game not found at:
    echo   %EXE%
    echo Edit WIN64 in this script to match your installation.
    pause
    exit /b 1
)

echo.
echo Starting Dreadnought  -^>  Revival Server at %GATEWAY_HOST%:%GATEWAY_PORT%
echo.

start "" /D "%WIN64%" "%EXE%" ^
    -GatewayAddress=%GATEWAY_HOST% -GatewayPort=%GATEWAY_PORT% ^
    -YFirmamentAddress=%YFIRMAMENT_HOST% -YFirmamentPort=%YFIRMAMENT_PORT% ^
    -log

endlocal
