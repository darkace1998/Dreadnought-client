# Dreadnought Revival — Client Mod

Tools for connecting **Dreadnought** (PC, UE4 4.13.1) to the
[Dreadnought Revival Server](https://github.com/darkace1998/Dreadnought-server).

Based on reverse engineering of `DreadGame-Win64-Shipping.exe` (Mar 2023 build)
and of the original `DreadnoughtLauncher.exe` (CEF/AngularJS app with a
password-protected embedded ZIP).

---

## How the original launcher authenticates (reverse-engineered)

The original `DreadnoughtLauncher.exe` is a CEF + AngularJS app. Its login
flow is:

1. Acquire a Steam **auth session ticket** via the bundled `steam.dll`.
2. POST the ticket as JSON-RPC `jwt.get.by_steam_ticket` to
   `https://profile-api.prod.greybox.sixfoot.live/auth/`
   (params: `ticket`, `appid: 835860`, `audience: "launcher"`,
   `realm: "dreadnought.pc-us"`, `client_name: "Dreadnought Launcher"`).
3. Decode the returned JWT and gate launch on `user_groups` containing
   `DREADNOUGHT PLAYER` or `DREADNOUGHT EMPLOYEE`. **The JWT itself is
   discarded after this check.**
4. Spawn `Launcher_DreadGame-Win64-Shipping.exe` with the command line:
   ```
   DreadGame ?version=… -pak
     -GatewayAddress=<host>  -GatewayPort=<port>
     -YFirmamentAddress=<host> -YFirmamentPort=<port>
   ```
5. The game's `WebServicesPlugin` **re-authenticates with the Steam ticket
   directly** against `POST <Gateway>/api/v1/authentication/login` —
   independent of the JWT.

Key insight: the launcher's profile-api JWT only gates launch UI. The
game and the revival server need only the four `-GatewayAddress` /
`-GatewayPort` / `-YFirmamentAddress` / `-YFirmamentPort` args plus a
working `steam_appid.txt`; the game does its own Steam-ticket
authentication.

---

## How the game connects to a server

The game's **WebServicesPlugin** reads four UE4 command-line args via
`FParse::Value()`:

| Argument | Example | Purpose |
|----------|---------|---------|
| `-GatewayAddress=HOST`     | `-GatewayAddress=10.0.0.1`   | REST gateway host |
| `-GatewayPort=PORT`        | `-GatewayPort=8080`          | REST gateway port |
| `-YFirmamentAddress=HOST`  | `-YFirmamentAddress=10.0.0.1`| MMOG WebSocket host |
| `-YFirmamentPort=PORT`     | `-YFirmamentPort=9000`       | MMOG WebSocket port (server `mmog_port`) |

REST endpoints used (with `Authorization: Bearer <token>` after login):

| Method | Path | Purpose |
|--------|------|---------|
| `GET`  | `/api/v1/ping`                         | Health check |
| `POST` | `/api/v1/authentication/login`         | Create session from Steam ticket |
| `POST` | `/api/v1/authentication/logout`        | Destroy session |
| `POST` | `/api/v1/session/touch`                | Keepalive |
| `GET`  | `/api/v1/account/legal`                | EULA check |
| `POST` | `/api/v1/account/legal/attest`         | Accept EULA |
| `GET`  | `/api/v1/bundles`                      | Market bundles |
| `GET`  | `/api/v1/catalog/...`                  | Item catalogs |
| `GET`  | `/api/v1/play`                         | MMOG server address + port |

After the REST handshake the game connects to the MMOG server via the
proprietary `YMmogbrain` WebSocket protocol; in-match traffic uses UE4
UDP on port **7777**.

See [`docs/connection-protocol.md`](docs/connection-protocol.md).

---

## Quickstart — launching via Steam

`DreadnoughtLauncher.exe` (built from this repo) replaces the original
launcher in the Dreadnought root, so clicking **Play** in Steam runs the
revival setup automatically.

1. Start the revival server (see
   [Dreadnought-server](https://github.com/darkace1998/Dreadnought-server)).

2. Build `Dreadnought-client.sln` (Release | x64):
   - `DreadnoughtLauncher.exe` — Steam launcher replacement
   - `Dreadnought-client.dll` — EAC popup-suppression hook
   - `wer.dll` — thin loader shim for the DLL

3. Copy `x64/Release/DreadnoughtLauncher.exe` to the Dreadnought root
   directory (overwriting the original).

4. Copy `launcher/revival.ini` next to it and edit:
   ```ini
   [Revival]
   GatewayHost=10.0.0.73
   GatewayPort=8080
   ; YFirmamentHost defaults to GatewayHost
   YFirmamentPort=9000   ; match server's mmog_port
   ```

5. Install the EAC-bypass DLL (recommended — Easy Anti-Cheat's servers
   are dead, so without this the game will show an error dialog at
   startup):
   - `wer/x64/Release/wer.dll` → `<Win64>\wer.dll`
   - `x64/Release/Dreadnought-client.dll` → `<Win64>\Dreadnought-client.dll`

   **Do not** rename `Dreadnought-client.dll` to `wer.dll`; place the
   dedicated shim. Renaming causes `0xc000007b` because the real WER
   exports are missing.

6. Click **Play** in Steam. The launcher will:
   - Ensure `steam_appid.txt` (containing `835860`) exists in `Win64\`.
   - Spawn `DreadGame-Win64-Shipping.exe` with the four `-Gateway*` /
     `-YFirmament*` args and `-log`.

   UE4 log: `%LOCALAPPDATA%\DreadGame\Saved\Logs\DreadGame.log`. Look
   for `LogWebServicesPlugin` entries.

---

## Fallback — launching without Steam

If you can't use Steam, use the fallback launchers. Edit the paths/
host inside, then run:

```bat
launch.bat
```

```powershell
.\launch.ps1 -GatewayHost 10.0.0.73 -GatewayPort 8080
```

Both scripts pass the same `-GatewayAddress` / `-GatewayPort` /
`-YFirmamentAddress` / `-YFirmamentPort` args as the launcher EXE.

---

## DLL mod (EAC popup suppression)

`Dreadnought-client.dll` is intentionally minimal — its only job is to
hook the EasyAntiCheat error-dialog routine at module-relative offset
`0x29FD910` and make it a no-op so launch proceeds when EAC's servers
can't be reached.

Removed compared to earlier community builds (we just want Dreadnought
Revival working — multiplayer auth, hangar, etc. are handled by the
game's own UI talking to the revival server):

- ImGui / D3D11 / Kiero overlay
- Revival Server / Multiplayer / Tutorial / Singleplayer / Profile tabs
- `ProcessEvent` and `UGameEngine::Tick` hooks
- Loadout/profile management (`UserProfiles.h`)
- UE4 SDK headers
- The old "rename `EasyAntiCheat_x64.dll` to `.bak`" trick. The
  in-process popup hook is enough; nothing on disk gets renamed.

### Requirements

- Visual Studio 2022 (v143 toolset, Windows 10 SDK)
- Dreadnought installed

### Building

1. Open `Dreadnought-client.sln` in Visual Studio 2022.
2. Select **Release | x64** and build all.
3. Outputs in `x64/Release/`:
   - `DreadnoughtLauncher.exe` — Steam launcher
   - `Dreadnought-client.dll` — EAC popup hook
   - `wer.dll` — WER-export loader shim

### DLL installation

Place both files in
`<Dreadnought install>\DreadGame\DreadGame\Binaries\Win64\`:
- `wer.dll` (the shim) — Windows loads this first; it stubs the WER
  exports and loads the real DLL from the same directory.
- `Dreadnought-client.dll` — does the EAC hook.

---

## Repository structure

```
Dreadnought-client/
 launch.bat                  # Fallback batch launcher (non-Steam)
 launch.ps1                  # Fallback PowerShell launcher (non-Steam)
 launcher/
   ├── DreadnoughtLauncher.cpp   # Steam launcher EXE source
   ├── DreadnoughtLauncher.vcxproj
   └── revival.ini               # Server config (copy next to EXE)
 src/
   ├── dllmain.cpp             # Minimal DLL — EAC popup hook only
   ├── pch.h / framework.h     # Precompiled header
   └── kiero/minhook/          # MinHook (only the function-hook engine)
 wer/
   ├── dllmain.cpp             # wer.dll shim — WER stubs + loads Dreadnought-client.dll
   ├── exports.def
   ├── pch.h / pch.cpp
   └── wer.vcxproj
 docs/
   ├── integration.md
   └── connection-protocol.md
 Dreadnought-client.vcxproj
 Dreadnought-client.sln
 README.md
```

---

## License

Educational and preservation purposes only.
Dreadnought and all related assets are property of their respective owners.
