# EAC Bypass Patch — Dreadnought Revival

## What This Does

Replaces `EasyAntiCheat_x64.dll` in the game's `Binaries\Win64` folder with a **stub DLL** that exports all EAC SDK functions as no-ops returning success.

### Two-Layer EAC Bypass

| Layer | Where | Mechanism |
|---|---|---|
| **Layer 1** | `Dreadnought.dll` (in-process hook) | Hooks EAC error handler at binary offset `0x29FD910` (client) / `0x1A841C0` (server) — suppresses the EAC not-found popup at runtime. See `dllmain.cpp:1336–1530`. |
| **Layer 2** | This stub DLL | Replaces the real `EasyAntiCheat_x64.dll` so all EAC SDK calls return success before the game binary even runs. |

Both layers are required because the game imports from the DLL before `Dreadnought.dll` is loaded.

## Files

| File | Description |
|---|---|
| `eac_bypass.cpp` | Stub C++ source — exports all EAC legacy + EOS functions |
| `eac_bypass.vcxproj` | Visual Studio 2019+ project — builds `EasyAntiCheat_x64.dll` |

## How to Build (Windows, Visual Studio 2019+)

```bat
msbuild eac_bypass.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\EasyAntiCheat_x64.dll`

## Deployment

1. Back up the original:
   ```bat
   cd DreadGame\DreadGame\Binaries\Win64
   rename EasyAntiCheat_x64.dll EasyAntiCheat_x64.dll.original
   ```

2. Copy the stub:
   ```bat
   copy <build_output>\EasyAntiCheat_x64.dll DreadGame\DreadGame\Binaries\Win64\
   ```

3. The existing `wer.dll` shim will load `Dreadnought.dll` which also installs the in-process EAC hook as a second-layer safety net.

## Exported Functions

### Legacy EAC SDK
- `EAC_InitGame` — returns 0 (success)
- `EAC_Authenticate` — returns 0
- `EAC_ShutdownGame` — returns 0
- `EAC_ProcessEvents` — returns 0

### EOS EAC Client (EOS SDK, post-2020)
- `EOS_EasyAntiCheat_Client_CreateContext` — returns non-NULL dummy handle
- `EOS_EasyAntiCheat_Client_DestroyContext` — returns 0
- `EOS_EasyAntiCheat_Client_Initialize` — returns 0
- `EOS_EasyAntiCheat_Client_Shutdown` — returns 0
- `EOS_EasyAntiCheat_Client_PollStatus` — returns 0 (no violations)
- `EOS_EasyAntiCheat_Client_BeginSession` — returns 0
- `EOS_EasyAntiCheat_Client_EndSession` — returns 0
- `EOS_EasyAntiCheat_Client_GetProtectedDirectoryCount` — returns 0
- `EOS_EasyAntiCheat_Client_GetProtectedDirectoryByIndex` — returns 0
- `EOS_EasyAntiCheat_Client_ReceiveMessageFromServer` — returns 0
- `EOS_EasyAntiCheat_Client_SendMessageToServer` — returns 0
- `EOS_EasyAntiCheat_Client_AddNotifyMessageToPeer` — returns 0
- `EOS_EasyAntiCheat_Client_RemoveNotifyMessageToPeer` — no-op
- `EOS_EasyAntiCheat_Client_AddNotifyPeerActionRequired` — returns 0
- `EOS_EasyAntiCheat_Client_RemoveNotifyPeerActionRequired` — no-op

## Notes

- `[INFERRED]` The exact export set was inferred from known UE4+EOS EAC SDK integration patterns and the Dreadnought binary analysis. If the game binary crashes on startup after applying this patch, run `dumpbin /IMPORTS DreadGame-Win64-Shipping.exe | findstr EasyAnti` on Windows to get the exact import list and compare against exports above.
- Built with `/MT` (static CRT) so the DLL has no external runtime dependencies.
- If `Dreadnought.dll` installs the in-process hook at `0x29FD910` *before* the EAC DLL export is called, this stub becomes redundant for that code path — but it remains important for any EAC DLL imports that occur at module load time (before MinHook can be set up).
