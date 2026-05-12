# Dreadnought Revival — Client Mod

A C++ DLL mod for **Dreadnought** (PC) that restores multiplayer functionality
by connecting to the [Dreadnought Revival Server](https://github.com/darkace1998/Dreadnought-server).

Based on the community patch by gwog et al., extended with a full HTTP API
client so players can register, track progression, and report match results
to a self-hosted revival server.

---

## Features

| Feature | Description |
|---------|-------------|
| **ImGui overlay** | In-game menu (F1) with tabbed UI |
| **Revival Server tab** | Register/login, view level/credits/GP, browse fleet |
| **Multiplayer tab** | Enter game server IP and connect |
| **Singleplayer tab** | Launch offline matches with bot counts and difficulty |
| **Profiles tab** | Manage named ship loadout presets |
| **Server API** | WinInet HTTP client — no extra dependencies |
| **Session tracking** | Match sessions reported to revival server automatically |
| **EAC bypass** | Suppresses EAC error popups so the game runs offline |

---

## Requirements

- **Dreadnought** installed (Steam or standalone)
- **Visual Studio 2022** (v143 toolset, Windows 10 SDK)
- A running instance of [Dreadnought-server](https://github.com/darkace1998/Dreadnought-server)
  (defaults to `http://127.0.0.1:8080`)

---

## Building

1. Open `Dreadnought-client.sln` in Visual Studio 2022.
2. Select **Release | x64**.
3. Build → the output is `x64/Release/Dreadnought-client.dll`.

> **Note:** The SDK headers (`src/SDK/`) are pre-generated from the UE4 SDK dump
> included in the community patch. No additional SDK setup is needed.

---

## Installation

1. Copy `Dreadnought-client.dll` to  
   `<Dreadnought install>\DreadGame\DreadGame\Binaries\Win64\`
2. Rename it to `wer.dll` (the shim loader expects this name).
3. Start Dreadnought normally.
4. Press **F1** to open the overlay.

---

## Connecting to the Revival Server

1. In the overlay, go to the **Revival Server** tab.
2. Set the **API URL** to your server (default: `http://127.0.0.1:8080`).
3. Click **Check** to verify the server is online.
4. Enter your pilot name and click **Register / Login**.
5. Your level, credits, GP and fleet will load automatically.

> The revival server must be running before you launch the game.
> See [Dreadnought-server](https://github.com/darkace1998/Dreadnought-server) for setup.

---

## Playing a Match

### Multiplayer (game server required)
1. Have a host run the game server (dedicated or listen).
2. In the **Multiplayer** tab, enter the host's IP address.
3. Click **Connect** — the mod will register a session with the revival server
   and then issue the UE4 `open <IP>` command.

### Singleplayer / Offline
1. Go to the **Singleplayer** tab.
2. Choose a map, set bot counts (0–7 friendly / 0–8 enemy), pick difficulty.
3. Click **Launch**.

---

## Configuration

| File | Purpose |
|------|---------|
| `profiles.ini` | Saved loadout profiles (auto-created, excluded from git) |
| `mod_debug.log` | Runtime log written by the mod (excluded from git) |
| `cfg.txt` | If present, the DLL treats the process as a server (no ImGui) |

---

## Repository Structure

```
Dreadnought-client/
├── src/
│   ├── dllmain.cpp        # Main DLL — ImGui overlay + game hooks
│   ├── ServerAPI.h/.cpp   # HTTP API client (WinInet)
│   ├── UserProfiles.h     # Loadout preset manager
│   ├── includes.h         # DX11 / ImGui includes
│   ├── pch.h              # Precompiled header
│   ├── SDK.h              # UE4 SDK master header
│   ├── SDK/               # Generated UE4 SDK package files
│   ├── imgui/             # Dear ImGui (DX11 backend)
│   └── kiero/             # Kiero VMT hook + MinHook
├── docs/
│   └── integration.md     # Server ↔ client API reference
├── Dreadnought-client.vcxproj
├── Dreadnought-client.sln
└── README.md
```

---

## License

This project is for **educational and preservation purposes only**.
Dreadnought and all related assets are property of their respective owners.
