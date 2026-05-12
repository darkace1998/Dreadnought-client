// DreadnoughtLauncher.cpp
// Dreadnought Revival Launcher
//
// Place this EXE in the Dreadnought root directory (next to the original
// DreadnoughtLauncher.exe, replacing it).  Steam launches it when the
// user clicks Play.
//
// What it does:
//   1. Reads revival.ini for GatewayHost / GatewayPort
//   2. Disables EasyAntiCheat (renames EasyAntiCheat_x64.dll → .bak)
//   3. Launches DreadGame-Win64-Shipping.exe with -GatewayAddress/-GatewayPort
//
// Config file (revival.ini, same directory as this EXE):
//   [Revival]
//   GatewayHost=10.0.0.73
//   GatewayPort=8080

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
    std::string eacDll   = win64  + "EasyAntiCheat_x64.dll";
    std::string eacBak   = win64  + "EasyAntiCheat_x64.dll.bak";

    // Verify the game executable exists
    if (!FileExists(gameExe)) {
        ShowError("Game executable not found:\n" + gameExe +
                  "\n\nMake sure DreadnoughtLauncher.exe is in the "
                  "Dreadnought root directory.");
        return 1;
    }

    // Read server config (falls back to localhost if revival.ini is absent)
    std::string gwHost = ReadIni(iniPath, "Revival", "GatewayHost", "127.0.0.1");
    std::string gwPort = ReadIni(iniPath, "Revival", "GatewayPort", "8080");

    // --- EAC bypass ---
    // Rename EasyAntiCheat_x64.dll so the EAC plugin fails silently.
    // EAC servers have been offline since 2023; without this a popup
    // appears on every launch.
    if (FileExists(eacDll) && !FileExists(eacBak)) {
        if (!MoveFileA(eacDll.c_str(), eacBak.c_str())) {
            // Non-fatal: game still runs, user just sees the EAC popup
            MessageBoxA(nullptr,
                "Could not disable EasyAntiCheat.\n"
                "Run the launcher as Administrator to suppress the EAC popup.",
                "Dreadnought Revival Launcher", MB_ICONWARNING | MB_OK);
        }
    }

    // --- Launch game ---
    // Working directory is the Win64 folder so the game finds its assets.
    // GatewayAddress= and GatewayPort= are read by the game's
    // WebServicesPlugin via UE4's FParse::Value().
    char cmdLine[MAX_PATH * 2];
    snprintf(cmdLine, sizeof(cmdLine),
             "\"%s\" -GatewayAddress=%s -GatewayPort=%s -log",
             gameExe.c_str(), gwHost.c_str(), gwPort.c_str());

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
