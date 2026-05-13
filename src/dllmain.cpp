// Dreadnought Revival Client Mod (minimal)
//
// Loaded into DreadGame-Win64-Shipping.exe via the wer.dll shim.
//
// All overlay / single-player / tutorial / multiplayer / profile features
// from the older community mod have been removed — the revival flow is
// fully driven by the launcher (which passes -GatewayAddress / -GatewayPort
// / -YFirmamentAddress / -YFirmamentPort) and by the game's own
// WebServicesPlugin authentication against the revival server.
//
// This DLL is only responsible for the one thing the game can't do for
// itself when EAC's servers are offline: suppress the EasyAntiCheat error
// popup that would otherwise block startup.

#include "pch.h"

#include <Windows.h>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "kiero/minhook/include/MinHook.h"

// -----------------------------------------------------------------------
//  Logging
// -----------------------------------------------------------------------
static std::mutex g_logMutex;
static void LogToFile(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream f("mod_debug.log", std::ios::app);
    if (f) f << msg << "\n";
}

// -----------------------------------------------------------------------
//  EAC error popup hook
//
//  Offset 0x29FD910 in the March 2023 Dreadnought shipping build is the
//  routine that displays the "EasyAntiCheat could not contact server"
//  dialog.  We replace it with a no-op so launch proceeds silently when
//  the revival server (which doesn't speak the EAC protocol) is used.
// -----------------------------------------------------------------------
static void* g_origEACErrorMessage = nullptr;
static void* EACErrorMessageHook(void* /*p1*/, void* /*p2*/) {
    return nullptr;
}

// -----------------------------------------------------------------------
//  MainThread — installs the EAC hook and exits
// -----------------------------------------------------------------------
static void MainThread() {
    // Give the game a moment to finish loading its modules before we hook.
    Sleep(5000);

    uintptr_t moduleBase = (uintptr_t)GetModuleHandle(nullptr);
    if (!moduleBase) {
        LogToFile("[Init] GetModuleHandle returned null; aborting");
        return;
    }

    if (MH_Initialize() != MH_OK) {
        LogToFile("[Init] MH_Initialize failed");
        return;
    }

    void* target = (void*)(moduleBase + 0x29FD910);
    if (MH_CreateHook(target, (void*)EACErrorMessageHook,
                      &g_origEACErrorMessage) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        LogToFile("[Init] EAC error-message hook failed");
        return;
    }

    LogToFile("[Init] EAC error-message popup suppressed");
}

// -----------------------------------------------------------------------
//  DllMain
// -----------------------------------------------------------------------
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID /*lpReserved*/) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        std::thread(MainThread).detach();
        break;
    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
