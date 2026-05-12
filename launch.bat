@echo off
:: Dreadnought Revival Launcher
:: Reverse-engineered from DreadGame-Win64-Shipping.exe:
::   GatewayAddress= and GatewayPort= are parsed by the game's WebServicesPlugin
::   via UE4's FParse::Value() to locate the revival HTTP gateway server.
::   Steam must be running before launching — the game calls SteamAPI_Init()
::   which reads steam_appid.txt (835860) from the Win64 working directory.

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
:: EAC servers have been offline since 2023. Without this, a popup appears on every launch.
if exist "%EAC_DLL%" (
    if not exist "%EAC_DISABLED%" (
        echo [EAC] Disabling EasyAntiCheat (renaming DLL^) ...
        rename "%EAC_DLL%" "EasyAntiCheat_x64.dll.bak"
        if errorlevel 1 (
            echo [EAC] WARNING: Could not rename EAC DLL. Run as Administrator to disable the popup.
        ) else (
            echo [EAC] EasyAntiCheat disabled. Restore with: ren "%EAC_DISABLED%" EasyAntiCheat_x64.dll
        )
    ) else (
        echo [EAC] Already disabled.
    )
)

:: ---- Install mod DLL as wer.dll (DLL search-order hijack) ----
:: Windows loads wer.dll from the executable directory before the system copy.
:: This injects the mod into the game process on startup (EAC bypass + ImGui overlay).
set MOD_DLL=%~dp0Dreadnought-client.dll
if not exist "%MOD_DLL%" set MOD_DLL=%~dp0x64\Release\Dreadnought-client.dll
if exist "%MOD_DLL%" (
    copy /y "%MOD_DLL%" "%WIN64%\wer.dll" >NUL 2>&1
    if not errorlevel 1 (
        echo [Mod] wer.dll installed ^(mod DLL active^)
    ) else (
        echo [Mod] WARNING: Could not copy mod DLL to Win64 directory.
        echo        Run as Administrator, or manually copy Dreadnought-client.dll
        echo        to %WIN64% and rename it to wer.dll.
    )
) else (
    echo [Mod] NOTE: Dreadnought-client.dll not found — build the mod first.
    echo        Without it, EAC bypass hook and overlay will not be available.
    echo        The game can still connect to the revival server via -GatewayAddress.
)

:: ---- Ensure steam_appid.txt exists in the Win64 directory ----
:: The Steam SDK reads this file from the process working directory to know the AppID.
if not exist "%WIN64%\steam_appid.txt" (
    echo 835860 > "%WIN64%\steam_appid.txt"
    echo [Steam] Created steam_appid.txt with AppID 835860
)

:: ---- Check if Steam is running ----
tasklist /FI "IMAGENAME eq steam.exe" 2>NUL | find /I "steam.exe" >NUL
if errorlevel 1 (
    echo.
    echo WARNING: Steam does not appear to be running.
    echo Steam must be running for authentication to work.
    echo Start Steam, log in, then press any key to continue.
    echo Press Ctrl+C to cancel.
    pause
)

echo.
echo Starting Dreadnought  -^>  Revival Server at %GATEWAY_HOST%:%GATEWAY_PORT%
echo Log: %%LOCALAPPDATA%%\DreadGame\Saved\Logs\DreadGame.log
echo.

:: Launch game with Win64 as the working directory so steam_appid.txt is found.
:: GatewayAddress= and GatewayPort= are read by WebServicesPlugin via FParse::Value().
:: The mod DLL (wer.dll) also reads these args to configure its own server API URL.
start "" /D "%WIN64%" "%EXE%" -GatewayAddress=%GATEWAY_HOST% -GatewayPort=%GATEWAY_PORT% -log

endlocal
