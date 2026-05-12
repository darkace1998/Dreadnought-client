# Client ↔ Server Integration

This document describes how `Dreadnought-client.dll` communicates with the
[Dreadnought Revival Server](https://github.com/darkace1998/Dreadnought-server).

---

## Transport

All requests use **HTTP/1.1 over WinInet** (no TLS by default).  
The base URL is user-configurable in the overlay (default: `http://127.0.0.1:8080`).

All POST/DELETE bodies are JSON (`Content-Type: application/json`).  
Responses are JSON; the mod parses them with substring search (no external JSON lib).

---

## Startup Sequence

```
DLL_PROCESS_ATTACH
  └─ MainThread() [detached thread]
       ├─ kiero::init()            hook DX11 Present
       └─ CheckServerAsync()       GET /api/v1/server/status
```

When the overlay is first shown:

```
User clicks "Register / Login"
  └─ RegisterAndFetchAsync(name)
       ├─ POST /api/v1/players/register   → playerID
       ├─ POST /api/v1/auth/token         → bearer token (stored in ServerAPI)
       ├─ GET  /api/v1/progression/{id}   → level, xp, credits, gp
       └─ GET  /api/v1/hangar/{id}        → ship list
```

---

## API Endpoints Used

### Health check
```
GET /api/v1/server/status

Response 200:
{
  "status":       "ok",
  "playerCount":  42,
  "activeMatches": 3,
  "uptime":       "12h34m"
}
```
Used by: **"Check" button** in Revival Server tab, automatic check on DLL attach.

---

### Register / login player
```
POST /api/v1/players/register
{
  "username": "PilotName"
}

Response 200:
{
  "id":       "abc-123",
  "username": "PilotName",
  "level":    1,
  "xp":       0,
  "credits":  500,
  "gp":       0
}
```
If the username already exists the server returns the existing record.

---

### Auth token
```
POST /api/v1/auth/token
{
  "playerID": "abc-123"
}

Response 200:
{
  "token": "eyJ..."
}
```
The token is stored in `ServerAPI.authToken` and sent as  
`Authorization: Bearer <token>` on subsequent requests.

---

### Progression
```
GET /api/v1/progression/{playerID}

Response 200:
{
  "id":      "abc-123",
  "level":   5,
  "xp":      2400,
  "credits": 18500,
  "gp":      120
}
```

---

### Hangar
```
GET /api/v1/hangar/{playerID}

Response 200:
[
  {
    "shipID":    "dn-t3-medium",
    "shipClass": "Dreadnought",
    "role":      "Frontline",
    "tier":      "T3",
    "isLocked":  false
  },
  ...
]
```
Shown in the **Fleet** list inside the Revival Server tab.

---

### Create session (match start)
```
POST /api/v1/session/create
{
  "playerID": "abc-123",
  "map":      "Amirani",
  "mode":     "TDM"
}

Response 200:
{
  "sessionID": "sess-xyz",
  "map":       "Amirani",
  "mode":      "TDM",
  "startTime": "2024-01-01T00:00:00Z"
}
```
Called immediately before the UE4 `open <IP>` console command.

---

### End session
```
DELETE /api/v1/session/{sessionID}

Response 200:
{
  "message": "session ended"
}
```
Called when a match ends (EndMatch hook).

---

### Report match result
```
POST /api/v1/stats/{playerID}
{
  "kills":  5,
  "deaths": 2,
  "damage": 48200,
  "won":    true
}

Response 200:
{
  "message":  "stats updated",
  "xpEarned": 250,
  "newLevel": 6
}
```
Called after `EndSession`.

---

## Data Flow Diagram

```
Game process
  │
  ├─ [DLL attach]     CheckStatus ──────────────────────► GET /api/v1/server/status
  │
  ├─ [Register btn]   RegisterPlayer ──────────────────► POST /api/v1/players/register
  │                   GetAuthToken   ──────────────────► POST /api/v1/auth/token
  │                   GetProgression ──────────────────► GET  /api/v1/progression/{id}
  │                   GetHangar      ──────────────────► GET  /api/v1/hangar/{id}
  │
  ├─ [Connect btn]    CreateSession  ──────────────────► POST /api/v1/session/create
  │                   open <IP>      ──► UE4 travel
  │
  └─ [Match ends]     EndSession     ──────────────────► DELETE /api/v1/session/{id}
                      ReportMatch    ──────────────────► POST /api/v1/stats/{id}
```

---

## Error Handling

- All HTTP calls are **non-blocking** (run on `std::thread` with a callback).
- Failed calls log to `mod_debug.log` and update `g_apiStatusText`.
- HTTP errors (non-200) and WinInet failures both invoke the callback with `ok=false`.
- The overlay shows an error toast for load failures.

---

## Adding New API Calls

1. Add a method signature to `src/ServerAPI.h`.
2. Implement it in `src/ServerAPI.cpp` using `HttpGetAsync` or `HttpPostAsync`.
3. Call it from `src/dllmain.cpp` with a lambda callback.
4. Wire any new server endpoint in
   [Dreadnought-server/api/router.go](https://github.com/darkace1998/Dreadnought-server/blob/main/api/router.go).
