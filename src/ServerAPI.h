#pragma once

#include <string>
#include <functional>
#include <vector>

// -----------------------------------------------------------------------
//  ServerStatus — result of GET /api/v1/server/status
// -----------------------------------------------------------------------
struct ServerStatus {
    bool    online        = false;
    int     playerCount   = 0;
    int     activeMatches = 0;
    long long uptimeSeconds = 0;
};

// -----------------------------------------------------------------------
//  PlayerInfo — result of register / GET /api/v1/players/{id}
// -----------------------------------------------------------------------
struct PlayerInfo {
    std::string id;
    std::string username;
    int         level    = 1;
    int         xp       = 0;
    long long   credits  = 0;
    int         gp       = 0;
    int         freeXP   = 0;
};

// -----------------------------------------------------------------------
//  HangarShipEntry — one ship returned from GET /api/v1/hangar/{id}
// -----------------------------------------------------------------------
struct HangarShipEntry {
    std::string shipID;
    std::string shipClass;
    std::string role;
    std::string tier;
};

// -----------------------------------------------------------------------
//  SessionInfo — result of POST /api/v1/session/create
// -----------------------------------------------------------------------
struct SessionInfo {
    std::string sessionID;
    bool        created = false;
};

// -----------------------------------------------------------------------
//  Callback types
// -----------------------------------------------------------------------
using StatusCallback   = std::function<void(bool ok, ServerStatus)>;
using PlayerCallback   = std::function<void(bool ok, PlayerInfo)>;
using HangarCallback   = std::function<void(bool ok, std::vector<HangarShipEntry>)>;
using SessionCallback  = std::function<void(bool ok, SessionInfo)>;
using RawCallback      = std::function<void(bool ok, std::string body)>;

// -----------------------------------------------------------------------
//  ServerAPI — thin async HTTP client that talks to Dreadnought-server.
//  Uses WinInet (ships with Windows, no extra deps needed).
//  All public methods spawn a background thread and call the callback
//  when done.  Callbacks may be invoked from any thread.
// -----------------------------------------------------------------------
class ServerAPI {
public:
    // Base URL of the Go revival server, e.g. "http://192.168.1.10:8080"
    std::string baseURL;

    // Set after a successful RegisterPlayer() or AuthToken() call
    std::string authToken;
    std::string playerID;

    explicit ServerAPI(const std::string& url = "http://127.0.0.1:8080");

    // ---- Core API calls (all async) ------------------------------------

    // GET /api/v1/server/status
    void CheckStatus(StatusCallback cb);

    // POST /api/v1/players/register  {"username":"<name>"}
    void RegisterPlayer(const std::string& username, PlayerCallback cb);

    // GET /api/v1/players/{playerID}
    void GetPlayerInfo(const std::string& pid, PlayerCallback cb);

    // POST /api/v1/auth/token  {"player_id":"<id>"}
    void GetAuthToken(const std::string& pid, RawCallback cb);

    // GET /api/v1/progression/{playerID}
    void GetProgression(const std::string& pid, PlayerCallback cb);

    // GET /api/v1/hangar/{playerID}
    void GetHangar(const std::string& pid, HangarCallback cb);

    // POST /api/v1/session/create
    void CreateSession(const std::string& pid,
                       const std::string& mapName,
                       const std::string& mode,
                       SessionCallback cb);

    // DELETE /api/v1/session/{sessionID}
    void EndSession(const std::string& sid, RawCallback cb);

    // POST /api/v1/stats/{playerID}  (simple match result)
    void ReportMatchResult(const std::string& pid,
                           int kills, int deaths,
                           int damageDone, bool won,
                           RawCallback cb);

private:
    // Synchronous HTTP helpers (called from worker threads)
    std::string HttpGet(const std::string& path);
    std::string HttpPost(const std::string& path, const std::string& jsonBody);
    std::string HttpDelete(const std::string& path);

    // Minimal JSON field extractors (no third-party JSON lib needed)
    std::string ParseJsonString(const std::string& json, const std::string& key);
    long long   ParseJsonInt64(const std::string& json, const std::string& key, long long def = 0);
    int         ParseJsonInt(const std::string& json, const std::string& key, int def = 0);
    bool        ParseJsonBool(const std::string& json, const std::string& key, bool def = false);

    // Parse an array of ship objects from hangar JSON
    std::vector<HangarShipEntry> ParseHangarShips(const std::string& json);

    // Build the full URL from a path
    std::string MakeURL(const std::string& path);

    // Parse host/port from baseURL for WinInet
    bool ParseBaseURL(std::string& host, int& port, bool& https);
};
