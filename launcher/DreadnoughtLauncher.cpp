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
#include <wininet.h>
#include <string>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "wininet.lib")

#define CLIENT_VERSION "1.0.0"
// [INFERRED] Version format matches server config.go version field

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

static void PrintWarning(const std::string& msg) {
    std::fprintf(stdout, "%s\n", msg.c_str());
    std::fflush(stdout);
}

static std::string ExtractJsonStringField(const std::string& json, const char* field) {
    const std::string needle = std::string("\"") + field + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return "";
    }
    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) {
        return "";
    }
    size_t valuePos = json.find('"', colonPos + 1);
    if (valuePos == std::string::npos) {
        return "";
    }
    ++valuePos;

    std::string value;
    bool escaped = false;
    for (size_t i = valuePos; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return "";
}

static bool HttpGet(const std::string& url, std::string& responseBody) {
    responseBody.clear();

    HINTERNET internet = InternetOpenA("DreadnoughtRevivalLauncher", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) {
        return false;
    }

    DWORD timeoutMs = 3000;
    InternetSetOptionA(internet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(internet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(internet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    HINTERNET request = InternetOpenUrlA(internet, url.c_str(), nullptr, 0,
                                         INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                         0);
    if (!request) {
        InternetCloseHandle(internet);
        return false;
    }

    char buffer[1024];
    DWORD bytesRead = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        responseBody.append(buffer, bytesRead);
        bytesRead = 0;
    }

    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    return !responseBody.empty();
}

static void WarnOnServerVersionMismatch(const std::string& serverBase) {
    const std::string pingUrl = serverBase + "/api/v1/ping";
    std::string responseBody;
    if (!HttpGet(pingUrl, responseBody)) {
        PrintWarning("WARNING: Could not reach server at " + pingUrl);
        return;
    }

    std::string serverVersion = ExtractJsonStringField(responseBody, "server_version");
    if (serverVersion.empty()) {
        serverVersion = ExtractJsonStringField(responseBody, "version");
    }
    if (!serverVersion.empty() && serverVersion != CLIENT_VERSION) {
        PrintWarning("WARNING: Server version " + serverVersion +
                     " does not match client version " CLIENT_VERSION ". Proceeding anyway.");
    }
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
    std::string serverBase = "http://" + gwHost + ":" + gwPort;

    WarnOnServerVersionMismatch(serverBase);

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
