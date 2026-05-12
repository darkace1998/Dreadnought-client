// wer/dllmain.cpp
// Thin shim DLL placed as wer.dll in the game's Win64 directory.
//
// Windows loads wer.dll early in process startup (Windows Error Reporting).
// By placing this DLL in the same directory as DreadGame-Win64-Shipping.exe,
// Windows loads it before the system copy (DLL search-order hijack).
//
// This DLL:
//  1. Exports the four WER functions the game/runtime expect (empty stubs).
//     Without these exports the loader fails with 0xc000007b.
//  2. On DLL_PROCESS_ATTACH, loads Dreadnought-client.dll from the same
//     directory, which injects the full mod (EAC hook + ImGui overlay).

#include "pch.h"
#include <Windows.h>

static DWORD WINAPI LoadModThread(LPVOID lpParam) {
    HMODULE hSelf = static_cast<HMODULE>(lpParam);

    char dir[MAX_PATH] = {};
    if (GetModuleFileNameA(hSelf, dir, MAX_PATH) == 0)
        return 1;

    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = '\0';

    char modPath[MAX_PATH];
    snprintf(modPath, MAX_PATH, "%sDreadnought-client.dll", dir);

    LoadLibraryA(modPath); // mod initialises itself via its own DllMain
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, LoadModThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}

// ---- WER stub exports -------------------------------------------------------
// These must exist so any static import of wer.dll resolves correctly.
// They are never actually called in a revival context.
extern "C" __declspec(dllexport) void WerReportCreate()       {}
extern "C" __declspec(dllexport) void WerReportAddFile()      {}
extern "C" __declspec(dllexport) void WerReportSetParameter() {}
extern "C" __declspec(dllexport) void WerReportSubmit()       {}
