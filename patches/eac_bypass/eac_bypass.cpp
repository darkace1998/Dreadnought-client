// EAC Bypass Stub DLL for Dreadnought Revival
//
// Purpose:
//   This DLL replaces the real EasyAntiCheat_x64.dll in the game's Binaries\Win64
//   directory. All EAC SDK function exports are stubbed to return "success" values
//   so the game initializes normally without EAC enforcement.
//
// How EAC works in Dreadnought:
//   - The game binary imports functions from EasyAntiCheat_x64.dll at startup.
//   - Without EAC connectivity to the live server, these calls time out or error,
//     causing the game to display an "EasyAntiCheat not found" popup and then
//     disconnect the player.
//   - The existing Dreadnought.dll mod hooks the in-process EAC error handler at
//     module offset 0x29FD910 (client) / 0x1A841C0 (server) to suppress the error
//     popup (see dllmain.cpp EACErrorMessageHook).
//   - This stub provides a second layer: the DLL-level interception that makes the
//     game think EAC loaded and authenticated successfully.
//
// References:
//   - src/Documents/config/settings_and_flags.md §EasyAntiCheat Settings.json
//   - EAC SDK (legacy): EAC_InitGame, EAC_Authenticate, EAC_ShutdownGame
//   - EOS EAC client (newer): EOS_EasyAntiCheat_Client_*
//   - dllmain.cpp:1336-1530 (existing hook fallback)
//
// Deployment:
//   Copy output EasyAntiCheat_x64.dll to:
//     DreadGame\DreadGame\Binaries\Win64\EasyAntiCheat_x64.dll
//   (overwriting the original EAC DLL)
//
// [INFERRED] Exact EAC SDK version and export set inferred from known Dreadnought
//            binary analysis and common UE4 EAC integration patterns.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// DLL entry point
// ---------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule;
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// EAC SDK exports — Legacy / Steam integration
// These are the functions the Dreadnought game binary expects from
// EasyAntiCheat_x64.dll under the old (pre-EOS) EAC SDK.
// All return 0 / EAC_Result_Success where applicable.
// ---------------------------------------------------------------------------

// EAC_Result enum values (legacy SDK):
//   0 = EAC_Result_Success
//   non-zero = various error codes

extern "C" {

// Initialize the EAC SDK context. Called at game startup.
// [INFERRED] Returns 0 = success.
__declspec(dllexport) int32_t EAC_InitGame(
    const char* pProductName,
    const char* pProductVersion,
    const char* pPublicKey,
    void*       pCallbackFn)
{
    (void)pProductName;
    (void)pProductVersion;
    (void)pPublicKey;
    (void)pCallbackFn;
    return 0; // EAC_Result_Success
}

// Authenticate the current player session.
// [INFERRED] Returns 0 = success without checking anything.
__declspec(dllexport) int32_t EAC_Authenticate(
    const char* pAuthToken,
    uint32_t    authTokenLength)
{
    (void)pAuthToken;
    (void)authTokenLength;
    return 0;
}

// Shut down EAC at game exit.
__declspec(dllexport) int32_t EAC_ShutdownGame() {
    return 0;
}

// Process / pump EAC events — called every frame by some games.
__declspec(dllexport) int32_t EAC_ProcessEvents() {
    return 0;
}

// ---------------------------------------------------------------------------
// EOS EAC Client exports — Epic Online Services integration.
// Dreadnought may have been updated to use EOS EAC before shutdown.
// Stubs return NULL handles or 0 = success as appropriate.
// [INFERRED] Based on EOS SDK public headers and known UE4 EOS EAC patterns.
// ---------------------------------------------------------------------------

// EOS handle type (opaque pointer).
typedef void* EOS_EasyAntiCheatClientHandle;

// EOS_EasyAntiCheat_Client_CreateContext
// Creates an EAC client context. Returns a non-NULL handle so the game
// doesn't treat it as a failure.
__declspec(dllexport) EOS_EasyAntiCheatClientHandle EOS_EasyAntiCheat_Client_CreateContext(
    void* pOptions)
{
    (void)pOptions;
    // Return a non-NULL "handle" (a dummy pointer) so the game doesn't fail.
    static uint8_t dummyCtx[16] = {0};
    return (EOS_EasyAntiCheatClientHandle)dummyCtx;
}

// EOS_EasyAntiCheat_Client_DestroyContext
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_DestroyContext(
    EOS_EasyAntiCheatClientHandle handle)
{
    (void)handle;
    return 0; // EOS_Success
}

// EOS_EasyAntiCheat_Client_Initialize
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_Initialize(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions)
{
    (void)handle;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_Shutdown
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_Shutdown(
    EOS_EasyAntiCheatClientHandle handle)
{
    (void)handle;
    return 0;
}

// EOS_EasyAntiCheat_Client_PollStatus
// Returns 0 = EOS_Success (no integrity violations).
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_PollStatus(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOutStatus)
{
    (void)handle;
    (void)pOutStatus;
    return 0;
}

// EOS_EasyAntiCheat_Client_BeginSession
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_BeginSession(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions)
{
    (void)handle;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_EndSession
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_EndSession(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions)
{
    (void)handle;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_GetProtectedDirectoryCount
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_GetProtectedDirectoryCount(
    EOS_EasyAntiCheatClientHandle handle)
{
    (void)handle;
    return 0;
}

// EOS_EasyAntiCheat_Client_GetProtectedDirectoryByIndex
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_GetProtectedDirectoryByIndex(
    EOS_EasyAntiCheatClientHandle handle,
    uint32_t index,
    void* pOptions)
{
    (void)handle;
    (void)index;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_ReceiveMessageFromServer
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_ReceiveMessageFromServer(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions)
{
    (void)handle;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_SendMessageToServer
__declspec(dllexport) int32_t EOS_EasyAntiCheat_Client_SendMessageToServer(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions)
{
    (void)handle;
    (void)pOptions;
    return 0;
}

// EOS_EasyAntiCheat_Client_AddNotifyMessageToPeer
__declspec(dllexport) uint64_t EOS_EasyAntiCheat_Client_AddNotifyMessageToPeer(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions,
    void* pClientData,
    void* pCallback)
{
    (void)handle;
    (void)pOptions;
    (void)pClientData;
    (void)pCallback;
    return 0;
}

// EOS_EasyAntiCheat_Client_RemoveNotifyMessageToPeer
__declspec(dllexport) void EOS_EasyAntiCheat_Client_RemoveNotifyMessageToPeer(
    EOS_EasyAntiCheatClientHandle handle,
    uint64_t notificationId)
{
    (void)handle;
    (void)notificationId;
}

// EOS_EasyAntiCheat_Client_AddNotifyPeerActionRequired
__declspec(dllexport) uint64_t EOS_EasyAntiCheat_Client_AddNotifyPeerActionRequired(
    EOS_EasyAntiCheatClientHandle handle,
    void* pOptions,
    void* pClientData,
    void* pCallback)
{
    (void)handle;
    (void)pOptions;
    (void)pClientData;
    (void)pCallback;
    return 0;
}

// EOS_EasyAntiCheat_Client_RemoveNotifyPeerActionRequired
__declspec(dllexport) void EOS_EasyAntiCheat_Client_RemoveNotifyPeerActionRequired(
    EOS_EasyAntiCheatClientHandle handle,
    uint64_t notificationId)
{
    (void)handle;
    (void)notificationId;
}

} // extern "C"
