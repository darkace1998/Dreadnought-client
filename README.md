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

## Quickstart

1. Start the revival server (see [Dreadnought-server](https://github.com/darkace1998/Dreadnought-server)).
2. Edit `GATEWAY_HOST` in `launch.bat` (or pass `-GatewayHost` to the PowerShell script) to point at your server.
3. Build the mod DLL (see **Building** below) and place `Dreadnought-client.dll` next to `launch.bat`.
4. Run the launcher **as Administrator** (needed to rename EAC DLL and write to the game directory):

**Batch:**
```bat
launch.bat
```

**PowerShell:**
```powershell
.\launch.ps1 -GatewayHost 10.0.0.1 -GatewayPort 8080
```

The launcher will automatically:
- Disable EasyAntiCheat (renames the DLL so the popup is suppressed)
- Copy `Dreadnought-client.dll` as `wer.dll` into the game's Win64 directory (DLL injection via search-order hijack)
- Create `steam_appid.txt` so SteamAPI_Init() finds the AppID
- Launch the game pointing at your revival server

The UE4 log is written to `%LOCALAPPDATA%\DreadGame\Saved\Logs\DreadGame.log`.
Search for `LogWebServicesPlugin` and `LogWebServiceRequest` entries.

Once in-game, press **F1** to toggle the mod overlay (server status, player info, single player / bots, loadout profiles).

---

## DLL mod (EAC bypass + overlay)

The C++ DLL (`Dreadnought-client.dll`) is needed to:

- **Bypass EAC** — hook `EACErrorMessageHook` so the game runs without official servers
- **Override server URL** — automatically reads `-GatewayAddress=` / `-GatewayPort=` from the game's command line so the ImGui overlay always talks to the same server as the game
- **ImGui overlay** — F1 panel for single player (bot matches), loadout profiles, and revival server status / player data

The DLL does **not** replicate the REST API — the game handles that natively via
the `-GatewayAddress=` / `-GatewayPort=` command-line arguments.

### Server URL resolution (priority order)

1. `-GatewayAddress=HOST -GatewayPort=PORT` on the game command line *(highest — set by launcher)*
2. `ServerURL=http://HOST:PORT` in `revival.cfg` next to the game executable
3. Default `http://127.0.0.1:8080`

The URL can also be changed at runtime in the **Revival Server** tab of the F1 overlay.

### Requirements

- Visual Studio 2022 (v143 toolset, Windows 10 SDK)
- Dreadnought installed

### Building

1. Open `Dreadnought-client.sln` in Visual Studio 2022.
2. Select **Release | x64** and build.
3. Output: `x64/Release/Dreadnought-client.dll`.

> The SDK headers (`src/SDK/`) are pre-generated from the UE4 SDK dump — no extra SDK setup needed.

### Installation (automated)

Place `Dreadnought-client.dll` (or `x64/Release/Dreadnought-client.dll`) next to
`launch.bat`. The launcher copies it as `wer.dll` into the Win64 directory automatically.

### Installation (manual)

1. Copy `Dreadnought-client.dll` to  
   `<Dreadnought install>\DreadGame\DreadGame\Binaries\Win64\`
2. Rename to `wer.dll` (DLL search-order hijack — loads before EAC check).
3. Launch via `launch.bat` or `launch.ps1`.

---

## Repository structure

```
Dreadnought-client/
 launch.bat                   # Windows batch launcher
 launch.ps1                   # PowerShell launcher
 src/
   ├── dllmain.cpp              # DLL — EAC bypass + optional ImGui overlay
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
