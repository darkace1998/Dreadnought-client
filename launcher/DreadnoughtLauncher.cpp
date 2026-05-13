// DreadnoughtLauncher.cpp
// Dreadnought Revival Launcher
//
// Replaces the original Yager/Six-Foot CEF launcher in the Dreadnought
// root directory.  Steam launches this when the user clicks Play.
//
// The original launcher (analyzed from the encrypted ZIP embedded in
// DreadnoughtLauncher.exe) does three things:
//
//   1. Get a Steam auth session ticket via steam.dll interop.
//   2. POST the ticket to profile-api/auth/ via JSON-RPC
//      `jwt.get.by_steam_ticket` and verify the returned JWT contains
//      the DREADNOUGHT PLAYER group  (this gate is offline-only now).
//   3. CreateProcess() Launcher_DreadGame-Win64-Shipping.exe with:
//        DreadGame ?version=<build> -pak
//          -GatewayAddress=<host> -GatewayPort=<port>
//          -YFirmamentAddress=<host> -YFirmamentPort=<port>
//
// The JWT is NEVER passed to the game — the game's WebServicesPlugin
// re-authenticates with the Steam ticket against
//   POST <GatewayAddress>:<GatewayPort>/api/v1/authentication/login
// So all this launcher needs to do is set the gateway/firmament args
// and start the shipping exe; the revival server handles auth itself.
//
// Config file (revival.ini, same directory as this EXE):
//   [Revival]
//   GatewayHost=10.0.0.73
//   GatewayPort=8080
//   YFirmamentHost=10.0.0.73      ; optional, defaults to GatewayHost
//   YFirmamentPort=48843          ; optional

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <string>
#include <cstdio>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

static std::string GetExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    return path;
}

// Read from INI file using Windows API (no external deps).
static std::string ReadIni(const std::string& file,
                            const std::string& section,
                            const std::string& key,
                            const std::string& def = "") {
    char buf[512] = {};
    GetPrivateProfileStringA(section.c_str(), key.c_str(), def.c_str(),
                             buf, sizeof(buf), file.c_str());
    return buf;
}

static bool FileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static void ShowError(const std::string& msg) {
    MessageBoxA(nullptr, msg.c_str(),
                "Dreadnought Revival Launcher", MB_ICONERROR | MB_OK);
}

// ---------------------------------------------------------------------------
//  WinMain — no console window
// ---------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    std::string exeDir = GetExeDir();

    // Paths relative to the Dreadnought root directory
    std::string iniPath  = exeDir + "revival.ini";
    std::string win64    = exeDir + "DreadGame\\DreadGame\\Binaries\\Win64\\";
    std::string gameExe  = win64  + "DreadGame-Win64-Shipping.exe";

    // Verify the game executable exists
    if (!FileExists(gameExe)) {
        ShowError("Game executable not found:\n" + gameExe +
                  "\n\nMake sure DreadnoughtLauncher.exe is in the "
                  "Dreadnought root directory.");
        return 1;
    }

    // Read server config (falls back to localhost if revival.ini is absent)
    std::string gwHost = ReadIni(iniPath, "Revival", "GatewayHost",    "127.0.0.1");
    std::string gwPort = ReadIni(iniPath, "Revival", "GatewayPort",    "8080");
    std::string fmHost = ReadIni(iniPath, "Revival", "YFirmamentHost", gwHost);
    std::string fmPort = ReadIni(iniPath, "Revival", "YFirmamentPort", "9000");

    // --- Ensure steam_appid.txt exists in Win64 ---
    // SteamAPI_Init() reads this file from the process working directory to
    // identify the AppID (835860 = Dreadnought) when not launched directly
    // by Steam's game tracking mechanism.  Without it the game logs
    // "Could not get auth token" and the login flow never completes.
    std::string steamAppIdFile = win64 + "steam_appid.txt";
    if (!FileExists(steamAppIdFile)) {
        HANDLE hFile = CreateFileA(steamAppIdFile.c_str(),
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            const char* appId = "835860";
            DWORD written = 0;
            WriteFile(hFile, appId, (DWORD)strlen(appId), &written, nullptr);
            CloseHandle(hFile);
        }
    }

    // --- Launch game ---
    // Working directory is the Win64 folder so SteamAPI_Init() finds
    // steam_appid.txt and the game finds its assets.
    //
    // -GatewayAddress / -GatewayPort   — REST gateway (WebServicesPlugin)
    // -YFirmamentAddress / -YFirmamentPort — MMOG WebSocket server
    //
    // Both pairs are read by the game via UE4's FParse::Value(), matching
    // the original launcher's workflow.json `launchGame` command line.
    char cmdLine[MAX_PATH * 4];
    snprintf(cmdLine, sizeof(cmdLine),
             "\"%s\" -GatewayAddress=%s -GatewayPort=%s "
             "-YFirmamentAddress=%s -YFirmamentPort=%s -log",
             gameExe.c_str(),
             gwHost.c_str(), gwPort.c_str(),
             fmHost.c_str(), fmPort.c_str());

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    if (!CreateProcessA(
            gameExe.c_str(),
            cmdLine,
            nullptr, nullptr,
            FALSE, 0,
            nullptr,
            win64.c_str(),   // working directory = Win64
            &si, &pi)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Failed to launch game (error %lu).", GetLastError());
        ShowError(msg);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
