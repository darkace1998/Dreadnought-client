@echo off
:: Dreadnought Revival Launcher
:: Reverse-engineered from DreadGame-Win64-Shipping.exe:
::   The game reads GatewayAddress= and GatewayPort= from the command line
::   via FParse::Value() to locate the WebServicesPlugin HTTP gateway.
::   No DLL injection is needed to redirect the REST API layer.
::   The DLL (Dreadnought-client.dll) is still required for EAC bypass.

setlocal

:: ---- Configure this to match your revival server ----
set GATEWAY_HOST=127.0.0.1
set GATEWAY_PORT=8080
set GAME_PATH=C:\Program Files (x86)\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64
:: -----------------------------------------------------

set EXE=%GAME_PATH%\DreadGame-Win64-Shipping.exe

if not exist "%EXE%" (
    echo ERROR: Game not found at %EXE%
    echo Edit GAME_PATH in this script to match your installation.
    pause
    exit /b 1
)

echo Starting Dreadnought with Revival Server at %GATEWAY_HOST%:%GATEWAY_PORT%
echo.

:: The game's WebServicesPlugin reads GatewayAddress= and GatewayPort= via FParse::Value().
:: -log enables the UE4 log file at: %%LOCALAPPDATA%%\DreadGame\Saved\Logs\DreadGame.log
start "" "%EXE%" -GatewayAddress=%GATEWAY_HOST% -GatewayPort=%GATEWAY_PORT% -log

endlocal
