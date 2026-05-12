# Dreadnought Revival — Client Mod

Tools for connecting **Dreadnought** (PC, UE4 4.13.1) to the
[Dreadnought Revival Server](https://github.com/darkace1998/Dreadnought-server).

Based on reverse engineering of `DreadGame-Win64-Shipping.exe` (Mar 2023 Shipping build).

---

## How the game connects to a server

The game's **WebServicesPlugin** reads two command-line arguments to locate
the HTTP gateway — no DLL injection is required for the REST API layer:

| Argument | Example | Source |
|----------|---------|--------|
| `-GatewayAddress=HOST` | `-GatewayAddress=10.0.0.1` | Binary: `FParse::Value("GatewayAddress=", ...)` |
| `-GatewayPort=PORT`    | `-GatewayPort=8080`        | Binary: `FParse::Value("GatewayPort=", ...)`    |

The game uses `Authorization: Bearer <token>` for all authenticated requests and
calls these endpoints (confirmed from binary UTF-16 string analysis):

| Method | Path | Purpose |
|--------|------|---------|
| `GET`  | `/api/v1/ping`                         | Health check |
| `POST` | `/api/v1/authentication/login`         | Create session |
| `POST` | `/api/v1/authentication/logout`        | Destroy session |
| `POST` | `/api/v1/session/touch`                | Keepalive |
| `GET`  | `/api/v1/account/legal`                | EULA check |
| `POST` | `/api/v1/account/legal/attest`         | Accept EULA |
| `POST` | `/api/v1/account/legal/document/accept`| Accept legal doc |
| `GET`  | `/api/v1/bundles`                      | Market bundles |
| `GET`  | `/api/v1/catalog/currency_pack_rmt`    | Real-money currency |
| `GET`  | `/api/v1/catalog/currency_pack_vc`     | Virtual currency |
| `GET`  | `/api/v1/catalog/digital_items_rmt`    | Real-money items |
| `GET`  | `/api/v1/catalog/digital_items_vc`     | VC items |
| `GET`  | `/api/v1/play`                         | MMOG server address + port |

After the REST handshake the game connects to the **MMOG server** (address from `/api/v1/play`)
via the proprietary `YMmogbrain` WebSocket protocol (`YA_*` messages).
Actual in-match traffic uses UE4 UDP on port **7777**.

See [`docs/connection-protocol.md`](docs/connection-protocol.md) for full protocol details.

---

## Quickstart — launching via Steam

**This is the recommended way.** The `DreadnoughtLauncher.exe` replaces the
original launcher so clicking **Play** in Steam runs the revival setup automatically.

### First-time setup

1. Start the revival server (see [Dreadnought-server](https://github.com/darkace1998/Dreadnought-server)).

2. Build both projects from `Dreadnought-client.sln` (Release | x64):
   - `DreadnoughtLauncher.exe` — Steam launcher replacement
   - `Dreadnought-client.dll` — optional overlay / EAC hook (rename to `wer.dll`)

3. Copy `x64/Release/DreadnoughtLauncher.exe` to the **Dreadnought root directory**  
   (same folder that already contains the original `DreadnoughtLauncher.exe`).  
   Overwrite / rename the original.

4. Copy `launcher/revival.ini` to the same Dreadnought root directory and edit it:
   ```ini
   [Revival]
   GatewayHost=10.0.0.73
   GatewayPort=8080
   ```

5. (Optional) Install the mod DLL for the ImGui overlay:  
   copy `Dreadnought-client.dll` to  
   `<Dreadnought root>\DreadGame\DreadGame\Binaries\Win64\`  
   and rename it `wer.dll`.

6. Click **Play** in Steam — the launcher will:
   - Disable EasyAntiCheat (renames `EasyAntiCheat_x64.dll` → `.bak` on first run — **run as Admin once**)
   - Launch `DreadGame-Win64-Shipping.exe` with `-GatewayAddress` and `-GatewayPort` pointing at your server

The UE4 log is at `%LOCALAPPDATA%\DreadGame\Saved\Logs\DreadGame.log`.
Search for `LogWebServicesPlugin` entries.

Once in-game, press **F1** to toggle the mod overlay (server status, player info, single player / bots, loadout profiles).

---

## Fallback — launching without Steam

Use `launch.bat` or `launch.ps1` if you need to launch outside Steam:

```bat
launch.bat
```

```powershell
.\launch.ps1 -GatewayHost 10.0.0.73 -GatewayPort 8080
```

---

## DLL mod (EAC bypass + overlay)

The C++ DLL (`Dreadnought-client.dll`, renamed to `wer.dll`) is optional but provides:

- **EAC bypass hook** — suppresses EasyAntiCheat error dialogs in-process
- **ImGui overlay** — F1 panel for single player (bot matches), loadout profiles, and revival server status / player data
- **Auto server URL** — reads `-GatewayAddress=` / `-GatewayPort=` from the game's command line so the overlay always talks to the same server as the game

### Server URL resolution (priority order)

1. `-GatewayAddress=HOST -GatewayPort=PORT` from command line *(set by launcher)*
2. `ServerURL=http://HOST:PORT` in `revival.cfg` next to the game executable
3. Default `http://127.0.0.1:8080`

The URL can also be changed at runtime in the **Revival Server** tab of the F1 overlay.

### Requirements

- Visual Studio 2022 (v143 toolset, Windows 10 SDK)
- Dreadnought installed

### Building

1. Open `Dreadnought-client.sln` in Visual Studio 2022.
2. Select **Release | x64** and build all.
3. Outputs:
   - `x64/Release/DreadnoughtLauncher.exe` — Steam launcher
   - `x64/Release/Dreadnought-client.dll` — optional overlay DLL

> The SDK headers (`src/SDK/`) are pre-generated from the UE4 SDK dump — no extra SDK setup needed.

### DLL installation

1. Copy `Dreadnought-client.dll` to  
   `<Dreadnought install>\DreadGame\DreadGame\Binaries\Win64\`
2. Rename to `wer.dll` (DLL search-order hijack — loaded by Windows before the system copy).

---

## Repository structure

```
Dreadnought-client/
 launch.bat                   # Fallback batch launcher (non-Steam)
 launch.ps1                   # Fallback PowerShell launcher (non-Steam)
 launcher/
   ├── DreadnoughtLauncher.cpp    # Steam launcher EXE source
   ├── DreadnoughtLauncher.vcxproj
   └── revival.ini                # Server config (copy to Dreadnought root)
 src/
   ├── dllmain.cpp              # DLL — EAC bypass + ImGui overlay
   ├── ServerAPI.h/.cpp         # HTTP API client (WinInet)
   ├── UserProfiles.h           # Loadout preset manager
   ├── includes.h               # DX11 / ImGui includes
   ├── pch.h                    # Precompiled header
   ├── SDK.h                    # UE4 SDK master header
   ├── SDK/                     # Generated UE4 SDK package files
   ├── imgui/                   # Dear ImGui (DX11 backend)
   └── kiero/                   # Kiero VMT hook + MinHook
 docs/
   ├── integration.md           # Server ↔ client API reference
   └── connection-protocol.md   # Reverse-engineered protocol documentation
 Dreadnought-client.vcxproj
 Dreadnought-client.sln
 README.md
```

---

## License

Educational and preservation purposes only.
Dreadnought and all related assets are property of their respective owners.

