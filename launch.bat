@echo off
:: Dreadnought Revival Launcher (batch fallback)
:: Use DreadnoughtLauncher.exe (via Steam) as the primary way to start the game.
:: This script is a fallback for launching outside of Steam.

setlocal

:: ---- Configure these to match your installation ----
set GATEWAY_HOST=127.0.0.1
set GATEWAY_PORT=8080
:: Path to the Win64 directory containing DreadGame-Win64-Shipping.exe
set WIN64=C:\Program Files (x86)\Steam\steamapps\common\Dreadnought\DreadGame\DreadGame\Binaries\Win64
:: -----------------------------------------------------

set EXE=%WIN64%\DreadGame-Win64-Shipping.exe
set EAC_DLL=%WIN64%\EasyAntiCheat_x64.dll
set EAC_DISABLED=%WIN64%\EasyAntiCheat_x64.dll.bak

if not exist "%EXE%" (
    echo ERROR: Game not found at:
    echo   %EXE%
    echo Edit WIN64 in this script to match your installation.
    pause
    exit /b 1
)

:: ---- EAC bypass: rename EasyAntiCheat_x64.dll so EAC plugin fails silently ----
if exist "%EAC_DLL%" (
    if not exist "%EAC_DISABLED%" (
        echo [EAC] Disabling EasyAntiCheat (renaming DLL^) ...
        rename "%EAC_DLL%" "EasyAntiCheat_x64.dll.bak"
        if errorlevel 1 (
            echo [EAC] WARNING: Could not rename EAC DLL. Run as Administrator.
        ) else (
            echo [EAC] Done. Restore with: ren "%EAC_DISABLED%" EasyAntiCheat_x64.dll
        )
    ) else (
        echo [EAC] Already disabled.
    )
)

echo.
echo Starting Dreadnought  -^>  Revival Server at %GATEWAY_HOST%:%GATEWAY_PORT%
echo.

start "" /D "%WIN64%" "%EXE%" -GatewayAddress=%GATEWAY_HOST% -GatewayPort=%GATEWAY_PORT% -log

endlocal
