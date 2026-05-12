# Dreadnought — Connection Protocol (Reverse Engineered)

This document describes the network protocol used by `DreadGame-Win64-Shipping.exe`
(UE4 4.13.1-1001076689, compiled March 2023) as determined by:

- Binary string analysis (`strings`, Python UTF-16 extraction)
- UE4 game log analysis (`logs1-3`, 2977 lines)
- Debug source-file paths embedded in the binary

---

## Two-layer network architecture

```
[Game client]
     │
     │  HTTP/JSON REST   (WebServicesPlugin)
     ├──────────────────► Revival Server :8080  (GatewayAddress / GatewayPort)
     │
     │  WebSocket/YA_*   (YMmogbrain / YFirmamentClient)
     ├──────────────────► MMOG Server :9000     (address from /api/v1/play)
     │
     │  UE4 UDP          (IpNetDriver)
     └──────────────────► Game Server :7777     (address from MMOG YA_RoomStart)
```

---

## Layer 1 — HTTP REST (WebServicesPlugin)

### Command-line arguments

The game reads the gateway address at startup via `FParse::Value()`:

```
-GatewayAddress=<hostname>    e.g. -GatewayAddress=10.0.0.1
-GatewayPort=<port>           e.g. -GatewayPort=8080
-MmogAddress=<host:port>      optional override for MMOG server
-log                          enable UE4 log file
```

### Connection flow

```
1. GET  /api/v1/ping
        → 200 OK  (server health check)

2. POST /api/v1/authentication/login
        body: { "steamid": "...", "ticket": "...", "platform": "steam" }
        resp: { "sessionId": "...", "token": "...", "playerId": "..." }

3. All subsequent requests include:
        Authorization: Bearer <token>

4. GET  /api/v1/account/legal
        resp: { "items": [], "accepted": true }   (EULA bypass — return empty)

5. POST /api/v1/account/legal/attest              (if legal items pending)

6. POST /api/v1/session/touch                     (periodic keepalive)

7. GET  /api/v1/bundles
8. GET  /api/v1/catalog/currency_pack_rmt
9. GET  /api/v1/catalog/currency_pack_vc
10. GET /api/v1/catalog/digital_items_rmt
11. GET /api/v1/catalog/digital_items_vc

12. GET /api/v1/play
        resp: { "address": "10.0.0.1", "port": 9000,
                "serverHost": "10.0.0.1", "serverPort": 9000 }
        → game connects to MMOG server
```

### Source files (from binary debug paths)

```
HttpWebServicesRequest.cpp
CreateSessionRequestDefinition.cpp
DestroySessionRequestDefinition.cpp
TouchSessionRequestDefinition.cpp
PingWebServicesRequestDefinition.cpp
GetLegalDocumentRequestDefinition.cpp
AcceptLegalItemRequestDefinition.cpp
GetMarketBundlesRequestDefinition.cpp
GetMarketCurrencyPackCatalogRequestDefinition.cpp
GetMarketItemCatalogRequestDefinition.cpp
GetMmogConnectionInfoRequestDefinition.cpp
```

---

## Layer 2 — MMOG WebSocket (YMmogbrain / YFirmamentClient)

### Overview

After the REST handshake, the game opens a WebSocket connection to the MMOG server.
All game-state messages use action names prefixed with `YA_`.

Source files in binary: `YFirmamentClient.cpp`, `mmog_client.cpp`, `OnlineSubsystemMmogbrain.cpp`

### Known message types (extracted from binary)

#### Session / Auth
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_UserLogin` | C→S | Begin session on MMOG server |
| `YA_UserLogout` | C→S | End session |
| `YA_SessionActive` | S→C | Session confirmed |
| `YA_SessionExpired` | S→C | Session timed out |
| `YA_SessionIdle` | S→C | Idle warning — must contain `SecondsUntilSuspend` |
| `YA_SessionSuspended` | S→C | Session suspended |

#### Fleet / Hangar
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_PlayerFleets` | S→C | Full fleet data |
| `YA_FleetUpdate` | S→C | Incremental fleet update |
| `YA_AddToFleet` | C→S | Add ship to fleet |
| `YA_RemoveFromFleet` | C→S | Remove ship |
| `YA_UpdateShipLoadout` | C→S | Change loadout modules |
| `YA_GetShipLoadout` | C→S | Request loadout |
| `YA_ShipLoadout` | S→C | Loadout data |
| `YA_CreateFleet` | C→S | Create new fleet |
| `YA_GetCommanderFleets` | C→S | Request available fleets |
| `YA_SetActiveFleet` | C→S | Select active fleet |
| `YA_ActiveFleet` | S→C | Confirmation of active fleet |

#### Matchmaking
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_EnterMatchmaking` | C→S | Join matchmaking queue |
| `YA_LeaveMatchmaking` | C→S | Leave queue |
| `YA_MatchmakingStatus` | S→C | Queue status update |
| `YA_QueueWaitingTime` | S→C | Estimated wait time |
| `YA_MatchFound` | S→C | Match found, pending acceptance |
| `YA_AcceptMatch` | C→S | Accept found match |
| `YA_DeclineMatch` | C→S | Decline found match |
| `YA_RoomStart` | S→C | Match starting — contains game server address |

#### Custom Rooms
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_CustomRoomCreate` | C→S | Create custom match room |
| `YA_CustomRoomInvite` | C→S | Invite player |
| `YA_CustomRoomJoin` | C→S | Join room |
| `YA_CustomRoomLeave` | C→S | Leave room |
| `YA_CustomRoomKick` | C→S | Kick player |
| `YA_CustomRoomSetReady` | C→S | Ready up |
| `YA_CustomRoomStartMatch` | C→S | Start the match |
| `YA_CustomRoomUpdate` | S→C | Room state update |
| `YA_CustomRoomDestroyed` | S→C | Room closed |
| `YA_CustomRoomSettings` | C→S | Change room settings |
| `YA_CustomRoomInviteList` | S→C | Pending invitations |

#### Social / Friends
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_FriendList` | S→C | Full friend list |
| `YA_FriendRequest` | C→S | Send friend request |
| `YA_AcceptFriendRequest` | C→S | Accept request |
| `YA_DeclineFriendRequest` | C→S | Decline request |
| `YA_RemoveFriend` | C→S | Remove friend |
| `YA_FriendOnline` | S→C | Friend came online |
| `YA_FriendOffline` | S→C | Friend went offline |
| `YA_PlayerStatus` | S→C | Player status update |

#### Chat
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_ChatMessage` | Both | Chat message |
| `YA_JoinChatChannel` | C→S | Join channel |
| `YA_LeaveChatChannel` | C→S | Leave channel |
| `YA_ChatHistory` | S→C | Chat history on join |
| `YA_SystemMessage` | S→C | Server system message |

#### Career / Progression
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_GetCareerProgression` | C→S | Request career data |
| `YA_CareerProgression` | S→C | Career data |
| `YA_ClaimCareerGoal` | C→S | Claim career reward |
| `YA_PlayerXP` | S→C | XP award |
| `YA_PlayerCredits` | S→C | Credits balance |
| `YA_PlayerLevel` | S→C | Level up notification |

#### Contracts / Daily Missions
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_GetDailyContractsData` | C→S | Request contracts |
| `YA_DailyContractsData` | S→C | Contract list |
| `YA_UpdateContract` | S→C | Contract progress update |

#### Officers / Cosmetics
| Action | Direction | Description |
|--------|-----------|-------------|
| `YA_GetOfficers` | C→S | Request officer list |
| `YA_Officers` | S→C | Officer data |
| `YA_AssignOfficer` | C→S | Assign officer to ship |
| `YA_GetCosmetics` | C→S | Request cosmetic items |
| `YA_Cosmetics` | S→C | Cosmetic data |

### Message format (partially known)

The exact wire format has not been confirmed. Based on the `YFirmamentClient.cpp`
source path and the `firmament.integration.greybox.aviary.cloud` hostname in the
binary, the protocol is likely **JSON over WebSocket (RFC 6455)**.

A typical message is expected to look like:
```json
{
  "action": "YA_UserLogin",
  "payload": {
    "sessionId": "...",
    "token": "..."
  }
}
```

> **TODO**: Confirm by running a local MITM proxy or analysing the `YFirmamentClient`
> plugin shared library if it exists separately.

---

## Layer 3 — UE4 UDP game server

Once `YA_RoomStart` is received, the game uses UE4's `IpNetDriver` to connect
to the game server address on **port 7777**.

From game logs:
```
LogNet: RemoteAddr: 5.5.5.5:7777, Name: IpNetDriver_0
LogNet: Connecting to server: 5.5.5.5:7777
```

This is standard Unreal Engine 4 UDP networking — a separate game server process
must be running at this address. The revival server returns this address as part of
the `YA_RoomStart` MMOG message.

---

## Live server reference

From binary analysis:

| Item | Value |
|------|-------|
| Gateway hostname | `firmament.integration.greybox.aviary.cloud` |
| Admin emails | `DNAdmin01@yager.de`, `DNAdmin02@yager.de` |
| Build date | March 1, 2023 |
| UE4 version | 4.13.1-1001076689 |
