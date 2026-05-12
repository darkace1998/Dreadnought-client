#include "pch.h"
#include "ServerAPI.h"

#include <windows.h>
#include <wininet.h>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "wininet.lib")

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------
ServerAPI::ServerAPI(const std::string& url)
    : baseURL(url) {
    // Remove trailing slash for consistency
    while (!baseURL.empty() && baseURL.back() == '/')
        baseURL.pop_back();
}

// -----------------------------------------------------------------------
//  URL parsing helpers
// -----------------------------------------------------------------------
bool ServerAPI::ParseBaseURL(std::string& host, int& port, bool& https) {
    std::string url = baseURL;
    https = false;
    port = 80;

    if (url.find("https://") == 0) {
        https = true;
        port = 443;
        url = url.substr(8);
    } else if (url.find("http://") == 0) {
        url = url.substr(7);
    }

    auto colonPos = url.rfind(':');
    if (colonPos != std::string::npos) {
        host = url.substr(0, colonPos);
        try { port = std::stoi(url.substr(colonPos + 1)); } catch (...) {}
    } else {
        host = url;
    }
    return !host.empty();
}

std::string ServerAPI::MakeURL(const std::string& path) {
    return baseURL + path;
}

// -----------------------------------------------------------------------
//  WinInet HTTP helpers (synchronous, called from worker threads)
// -----------------------------------------------------------------------
std::string ServerAPI::HttpGet(const std::string& path) {
    std::string host;
    int port = 80;
    bool https = false;
    if (!ParseBaseURL(host, port, https)) return "";

    HINTERNET hInternet = InternetOpenA("DreadnoughtRevival/1.0",
        INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), (INTERNET_PORT)port,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (https) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(),
        nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    // Add auth header if we have a token
    if (!authToken.empty()) {
        std::string authHeader = "Authorization: Bearer " + authToken;
        HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.size(),
            HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    }
    HttpAddRequestHeadersA(hRequest, "Accept: application/json\r\n", (DWORD)-1L,
        HTTP_ADDREQ_FLAG_ADD);

    if (!HttpSendRequestA(hRequest, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        response += buf;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
}

std::string ServerAPI::HttpPost(const std::string& path, const std::string& jsonBody) {
    std::string host;
    int port = 80;
    bool https = false;
    if (!ParseBaseURL(host, port, https)) return "";

    HINTERNET hInternet = InternetOpenA("DreadnoughtRevival/1.0",
        INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), (INTERNET_PORT)port,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (https) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(),
        nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "Content-Type: application/json\r\nAccept: application/json\r\n";
    if (!authToken.empty())
        headers += "Authorization: Bearer " + authToken + "\r\n";

    if (!HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(),
            (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size())) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        response += buf;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
}

std::string ServerAPI::HttpDelete(const std::string& path) {
    std::string host;
    int port = 80;
    bool https = false;
    if (!ParseBaseURL(host, port, https)) return "";

    HINTERNET hInternet = InternetOpenA("DreadnoughtRevival/1.0",
        INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), (INTERNET_PORT)port,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (https) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "DELETE", path.c_str(),
        nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    if (!authToken.empty()) {
        std::string authHeader = "Authorization: Bearer " + authToken + "\r\n";
        HttpAddRequestHeadersA(hRequest, authHeader.c_str(), (DWORD)authHeader.size(),
            HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    }

    if (!HttpSendRequestA(hRequest, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buf[1024];
    DWORD bytesRead = 0;
    while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        response += buf;
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
}

// -----------------------------------------------------------------------
//  Minimal JSON parsers — no external lib needed
// -----------------------------------------------------------------------
std::string ServerAPI::ParseJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

long long ServerAPI::ParseJsonInt64(const std::string& json, const std::string& key, long long def) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return def;
    // skip whitespace
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return def;
    try {
        size_t n = 0;
        long long val = std::stoll(json.substr(pos), &n);
        return val;
    } catch (...) { return def; }
}

int ServerAPI::ParseJsonInt(const std::string& json, const std::string& key, int def) {
    return (int)ParseJsonInt64(json, key, (long long)def);
}

bool ServerAPI::ParseJsonBool(const std::string& json, const std::string& key, bool def) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return def;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return def;
    if (json.substr(pos, 4) == "true") return true;
    if (json.substr(pos, 5) == "false") return false;
    return def;
}

std::vector<HangarShipEntry> ServerAPI::ParseHangarShips(const std::string& json) {
    std::vector<HangarShipEntry> ships;
    // Look for "ship_id" occurrences in the JSON array
    size_t pos = 0;
    while ((pos = json.find("\"ship_id\"", pos)) != std::string::npos) {
        HangarShipEntry e;
        // Extract a small window around this ship object
        size_t start = (pos > 200) ? pos - 200 : 0;
        size_t end   = (std::min)(json.size(), pos + 400);
        std::string chunk = json.substr(start, end - start);

        e.shipID    = ParseJsonString(chunk, "ship_id");
        e.shipClass = ParseJsonString(chunk, "class");
        e.role      = ParseJsonString(chunk, "role");
        e.tier      = ParseJsonString(chunk, "tier");

        if (!e.shipID.empty())
            ships.push_back(e);
        pos++;
    }
    return ships;
}

// -----------------------------------------------------------------------
//  Public async API methods
// -----------------------------------------------------------------------
void ServerAPI::CheckStatus(StatusCallback cb) {
    std::thread([this, cb]() {
        std::string body = HttpGet("/api/v1/server/status");
        ServerStatus s;
        if (!body.empty()) {
            s.online        = ParseJsonBool(body, "online");
            s.playerCount   = ParseJsonInt(body, "player_count");
            s.activeMatches = ParseJsonInt(body, "active_matches");
            s.uptimeSeconds = ParseJsonInt64(body, "uptime_seconds");
        }
        cb(!body.empty() && s.online, s);
    }).detach();
}

void ServerAPI::RegisterPlayer(const std::string& username, PlayerCallback cb) {
    std::thread([this, username, cb]() {
        std::string body = "{\"username\":\"" + username + "\"}";
        std::string resp = HttpPost("/api/v1/players/register", body);
        PlayerInfo p;
        bool ok = !resp.empty();
        if (ok) {
            p.id       = ParseJsonString(resp, "id");
            p.username = ParseJsonString(resp, "username");
            if (!p.id.empty()) {
                playerID = p.id;
            } else {
                ok = false;
            }
        }
        cb(ok, p);
    }).detach();
}

void ServerAPI::GetPlayerInfo(const std::string& pid, PlayerCallback cb) {
    std::thread([this, pid, cb]() {
        std::string resp = HttpGet("/api/v1/players/" + pid);
        PlayerInfo p;
        bool ok = !resp.empty();
        if (ok) {
            p.id       = ParseJsonString(resp, "id");
            p.username = ParseJsonString(resp, "username");
        }
        cb(ok, p);
    }).detach();
}

void ServerAPI::GetAuthToken(const std::string& pid, RawCallback cb) {
    std::thread([this, pid, cb]() {
        std::string body = "{\"player_id\":\"" + pid + "\"}";
        std::string resp = HttpPost("/api/v1/auth/token", body);
        bool ok = !resp.empty();
        if (ok) {
            std::string tok = ParseJsonString(resp, "token");
            if (!tok.empty()) authToken = tok;
        }
        cb(ok, resp);
    }).detach();
}

void ServerAPI::GetProgression(const std::string& pid, PlayerCallback cb) {
    std::thread([this, pid, cb]() {
        std::string resp = HttpGet("/api/v1/progression/" + pid);
        PlayerInfo p;
        bool ok = !resp.empty();
        if (ok) {
            p.id      = pid;
            p.level   = ParseJsonInt(resp, "level", 1);
            p.xp      = ParseJsonInt(resp, "xp", 0);
            p.credits = ParseJsonInt64(resp, "credits", 0);
            p.gp      = ParseJsonInt(resp, "gp", 0);
            p.freeXP  = ParseJsonInt(resp, "free_xp", 0);
        }
        cb(ok, p);
    }).detach();
}

void ServerAPI::GetHangar(const std::string& pid, HangarCallback cb) {
    std::thread([this, pid, cb]() {
        std::string resp = HttpGet("/api/v1/hangar/" + pid);
        bool ok = !resp.empty();
        std::vector<HangarShipEntry> ships;
        if (ok) ships = ParseHangarShips(resp);
        cb(ok, ships);
    }).detach();
}

void ServerAPI::CreateSession(const std::string& pid,
                              const std::string& mapName,
                              const std::string& mode,
                              SessionCallback cb) {
    std::thread([this, pid, mapName, mode, cb]() {
        std::string body = "{\"player_id\":\"" + pid +
                           "\",\"map\":\"" + mapName +
                           "\",\"mode\":\"" + mode + "\"}";
        std::string resp = HttpPost("/api/v1/session/create", body);
        SessionInfo s;
        bool ok = !resp.empty();
        if (ok) {
            s.sessionID = ParseJsonString(resp, "session_id");
            s.created   = !s.sessionID.empty();
            ok = s.created;
        }
        cb(ok, s);
    }).detach();
}

void ServerAPI::EndSession(const std::string& sid, RawCallback cb) {
    std::thread([this, sid, cb]() {
        std::string resp = HttpDelete("/api/v1/session/" + sid);
        cb(!resp.empty(), resp);
    }).detach();
}

void ServerAPI::ReportMatchResult(const std::string& pid,
                                  int kills, int deaths,
                                  int damageDone, bool won,
                                  RawCallback cb) {
    std::thread([this, pid, kills, deaths, damageDone, won, cb]() {
        std::string wonStr = won ? "true" : "false";
        std::string body =
            "{\"kills\":" + std::to_string(kills) +
            ",\"deaths\":" + std::to_string(deaths) +
            ",\"damage_done\":" + std::to_string(damageDone) +
            ",\"won\":" + wonStr + "}";
        std::string resp = HttpPost("/api/v1/stats/" + pid, body);
        cb(!resp.empty(), resp);
    }).detach();
}
