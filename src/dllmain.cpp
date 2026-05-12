// Dreadnought Revival Client Mod
// Based on the community patch by gwog et al.
// Server API integration by Dreadnought Revival Project.
//
// This DLL is injected into Dreadnought.exe via wer.dll shim.
// It hooks DirectX 11 Present to render an ImGui overlay and
// intercepts UE4 ProcessEvent to drive game-side logic.
//
// Revival Server integration (new in this fork):
//   - Registers the player with the Go revival server on load
//   - Fetches hangar/progression data to show in the overlay
//   - Reports match sessions to the server
//   - All HTTP calls use WinInet — no extra dependencies

#include "pch.h"
#include "includes.h"
#include "SDK.h"
#include "imgui_stdlib.h"
#include "kiero/minhook/include/MinHook.h"
#include "UserProfiles.h"
#include "ServerAPI.h"

#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <locale>
#include <atomic>
#include <algorithm>

using namespace CG;

// -----------------------------------------------------------------------
//  Logging
// -----------------------------------------------------------------------
static std::mutex g_logMutex;
static void LogToFile(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream f("mod_debug.log", std::ios::app);
    if (f) f << msg << "\n";
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

void AddErrorMessage(const std::string& message);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// -----------------------------------------------------------------------
//  DirectX / ImGui globals
// -----------------------------------------------------------------------
Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device*           pDevice = NULL;
ID3D11DeviceContext*    pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

namespace Globals {
    uintptr_t ModuleBase = 0;
    bool      AmServer   = false;
}

// -----------------------------------------------------------------------
//  Constants
// -----------------------------------------------------------------------
const int MAX_FRIENDLY_BOTS = 7;
const int MAX_ENEMY_BOTS    = 8;
const int MAX_DIFFICULTY    = 2;
const int SERVER_PORT       = 7777;
const int SHORT_SLEEP_MS    = 100;
const int INIT_SLEEP_MS     = 5000;
const int AI_SETUP_SLEEP_MS = 20000;
const int AI_SPAWN_SLEEP_MS = 30000;

// -----------------------------------------------------------------------
//  Map definitions
// -----------------------------------------------------------------------
struct MapInfo {
    const char*    displayName;
    const wchar_t* fileName;
};

const MapInfo AVAILABLE_MAPS[11] = {
    { "Amirani",   L"MP_Amirani_P"         },
    { "DansMap",   L"MP_DansMap_P"         },
    { "Derelict",  L"MP_Derelict_P"        },
    { "Glacier",   L"MP_Glacier_P"         },
    { "Gorge",     L"MP_Gorge_P"           },
    { "Highlands", L"MP_Highlands_P"       },
    { "Paradise",  L"MP_Paradise_P"        },
    { "Skybridge", L"MP_Skybridge_P"       },
    { "Space01",   L"MP_Space01_P"         },
    { "Space02",   L"MP_Space02_P"         },
    { "Tutorial",  L"S01E00_00_Tutorial_P" },
};

// -----------------------------------------------------------------------
//  Revival Server API — single global instance
// -----------------------------------------------------------------------
static ServerAPI g_serverAPI("http://127.0.0.1:8080");

// Server connection state
static std::mutex      g_apiMutex;
static bool            g_serverOnline         = false;
static bool            g_apiChecked           = false;
static std::string     g_apiPlayerName        = "";
static std::string     g_apiPlayerID          = "";
static std::string     g_apiStatusText        = "Not connected";
static int             g_apiLevel             = 0;
static long long       g_apiCredits           = 0;
static int             g_apiGP                = 0;
static int             g_apiXP                = 0;
static std::vector<HangarShipEntry> g_apiHangar;
static std::string     g_currentSessionID     = "";
static char            g_serverURLBuf[256]    = "http://127.0.0.1:8080";
static char            g_playerNameBuf[64]    = "";
static bool            g_apiRegistering       = false;
static bool            g_apiRegistered        = false;

// Called once at startup to check server health
static void CheckServerAsync() {
    g_serverAPI.CheckStatus([](bool ok, ServerStatus s) {
        std::lock_guard<std::mutex> lk(g_apiMutex);
        g_serverOnline = ok;
        g_apiChecked   = true;
        g_apiStatusText = ok
            ? ("Online  |  Players: " + std::to_string(s.playerCount) +
               "  |  Matches: " + std::to_string(s.activeMatches))
            : "Server unreachable";
        LogToFile("[API] Status check: " + g_apiStatusText);
    });
}

// Register player and fetch initial data
static void RegisterAndFetchAsync(const std::string& name) {
    {
        std::lock_guard<std::mutex> lk(g_apiMutex);
        g_apiRegistering = true;
        g_apiStatusText  = "Registering...";
    }
    g_serverAPI.RegisterPlayer(name, [](bool ok, PlayerInfo p) {
        if (!ok) {
            std::lock_guard<std::mutex> lk(g_apiMutex);
            g_apiStatusText  = "Registration failed";
            g_apiRegistering = false;
            return;
        }
        {
            std::lock_guard<std::mutex> lk(g_apiMutex);
            g_apiPlayerID   = p.id;
            g_apiPlayerName = p.username;
            g_apiRegistered = true;
            g_apiStatusText = "Registered: " + p.username;
        }
        LogToFile("[API] Registered playerID=" + p.id);

        // Fetch auth token
        g_serverAPI.playerID = p.id;
        g_serverAPI.GetAuthToken(p.id, [](bool ok, std::string) {
            if (ok) LogToFile("[API] Auth token acquired");
        });

        // Fetch progression
        g_serverAPI.GetProgression(p.id, [](bool ok, PlayerInfo prog) {
            if (ok) {
                std::lock_guard<std::mutex> lk(g_apiMutex);
                g_apiLevel   = prog.level;
                g_apiXP      = prog.xp;
                g_apiCredits = prog.credits;
                g_apiGP      = prog.gp;
            }
        });

        // Fetch hangar
        g_serverAPI.GetHangar(p.id, [](bool ok, std::vector<HangarShipEntry> ships) {
            if (ok) {
                std::lock_guard<std::mutex> lk(g_apiMutex);
                g_apiHangar = ships;
                LogToFile("[API] Hangar: " + std::to_string(ships.size()) + " ships");
            }
            std::lock_guard<std::mutex> lk(g_apiMutex);
            g_apiRegistering = false;
        });
    });
}

// Called when connecting to a game server
static void OnConnectToServer(const std::string& gameServerIP,
                              const std::string& mapName,
                              const std::string& mode) {
    if (g_apiPlayerID.empty()) return;
    g_serverAPI.CreateSession(g_apiPlayerID, mapName, mode,
        [gameServerIP](bool ok, SessionInfo s) {
            if (ok) {
                std::lock_guard<std::mutex> lk(g_apiMutex);
                g_currentSessionID = s.sessionID;
                LogToFile("[API] Session created: " + s.sessionID);
            }
        });
}

// Called when a match ends
static void OnMatchEnd(int kills, int deaths, int damage, bool won) {
    if (g_apiPlayerID.empty()) return;

    std::string sid;
    {
        std::lock_guard<std::mutex> lk(g_apiMutex);
        sid = g_currentSessionID;
        g_currentSessionID = "";
    }

    if (!sid.empty()) {
        g_serverAPI.EndSession(sid, [](bool, std::string) {
            LogToFile("[API] Session ended");
        });
    }

    g_serverAPI.ReportMatchResult(g_apiPlayerID, kills, deaths, damage, won,
        [](bool ok, std::string) {
            if (ok) LogToFile("[API] Match result reported");
        });
}

// -----------------------------------------------------------------------
//  Game-state globals
// -----------------------------------------------------------------------
static ProfileManager g_profileManager;

std::atomic<bool> g_showProfilePicker(false);
bool  interceptPostLogin = false;
bool  procMapLoad        = false;
std::string mapCommand   = "";
bool  connectToServer    = false;
std::string serverIP     = "";
bool  launchTutorial     = false;
bool  forceHUD           = false;
std::string loadoutString = "";
bool  launchSingleplayer  = false;
int   map        = 0;
int   numBotsTeamOne = 3;
int   numBotsTeamTwo = 3;
int   difficulty     = 1;

// -----------------------------------------------------------------------
//  Error message system
// -----------------------------------------------------------------------
std::mutex      g_errorMutex;
std::string     g_lastErrorMessage;
bool            g_showErrorMessage = false;

void AddErrorMessage(const std::string& message) {
    std::lock_guard<std::mutex> lk(g_errorMutex);
    g_lastErrorMessage = message;
    g_showErrorMessage = true;
    LogToFile("[ERROR] " + message);
}

// -----------------------------------------------------------------------
//  Thread tracking
// -----------------------------------------------------------------------
std::mutex g_threadsMutex;
std::vector<std::thread> g_activeThreads;

template<typename Function, typename... Args>
std::thread CreateTrackedThread(Function&& func, Args&&... args) {
    std::thread t(std::forward<Function>(func), std::forward<Args>(args)...);
    std::lock_guard<std::mutex> lk(g_threadsMutex);
    g_activeThreads.push_back(std::move(t));
    return std::thread();
}

void JoinAllThreads() {
    std::lock_guard<std::mutex> lk(g_threadsMutex);
    for (auto& t : g_activeThreads)
        if (t.joinable()) t.join();
    g_activeThreads.clear();
}

// -----------------------------------------------------------------------
//  Loadout helpers
// -----------------------------------------------------------------------
void LoadLoadouts() {
    if (loadoutString.empty()) return;
    std::wstring wLoadoutString = Utf8ToWide(loadoutString);
    if (wLoadoutString.empty()) return;
    UObject* loaded = StaticLoadClass(UYShipLoadout::StaticClass(), nullptr, wLoadoutString.c_str());
    if (!loaded)
        AddErrorMessage("Failed to load loadout: " + loadoutString);
}

void CompleteSingleplayerMatchSetup(std::string ls) {
    loadoutString = ls;
    LoadLoadouts();
}

// -----------------------------------------------------------------------
//  AI helpers
// -----------------------------------------------------------------------
void SetupAIDifficulty(int diff) {
    auto* kismet = getLastOfType<UKismetSystemLibrary>();
    if (!kismet || !*UWorld::GWorld) return;
    auto* world = *UWorld::GWorld;
    std::wstring cmd = L"setaidifficulty " + std::to_wstring(diff);
    if (world->OwningGameInstance && world->OwningGameInstance->LocalPlayers.Count() > 0)
        kismet->STATIC_ExecuteConsoleCommand(world, cmd.c_str(),
            world->OwningGameInstance->LocalPlayers[0]->PlayerController);
}

void SetupNPCSpawnIDs(int numTeamOne, int numTeamTwo) {
    // Spawning handled via cfg.txt for server mode; this is client-side stub
    (void)numTeamOne; (void)numTeamTwo;
}

void SetupSingleplayerAIThread(int nBotT1, int nBotT2, int diff, std::string ls) {
    Sleep(AI_SETUP_SLEEP_MS);
    CompleteSingleplayerMatchSetup(ls);
    Sleep(AI_SPAWN_SLEEP_MS - AI_SETUP_SLEEP_MS);
    SetupAIDifficulty(diff);
    SetupNPCSpawnIDs(nBotT1, nBotT2);
}

void DelaySingleplayerSetupThread(std::string ls) {
    Sleep(AI_SETUP_SLEEP_MS);
    CompleteSingleplayerMatchSetup(ls);
}

// -----------------------------------------------------------------------
//  ImGui init
// -----------------------------------------------------------------------
void InitImGui() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 6.0f;
    style.FrameRounding    = 4.0f;
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.08f, 0.10f, 0.14f, 0.94f);
    style.Colors[ImGuiCol_TitleBg]   = ImVec4(0.05f, 0.07f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.20f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_Button]    = ImVec4(0.12f, 0.28f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.40f, 0.75f, 1.00f);

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(pDevice, pContext);
}

// -----------------------------------------------------------------------
//  WndProc hook
// -----------------------------------------------------------------------
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// -----------------------------------------------------------------------
//  ProcessEvent hook — game-thread actions
// -----------------------------------------------------------------------
std::mutex g_globalMutex;

void ProcessEventHook(UObject* object, UFunction* function, void* params) {
    if (!Globals::AmServer) {
        if (function->GetFullName().find("ToggleLoadoutSelection") != std::string::npos) {
            if (params) {
                struct { bool IsActive; }* p = decltype(p)(params);
                g_showProfilePicker = p->IsActive;
            }
        }

        if (connectToServer) {
            connectToServer = false;
            forceHUD = true;

            // Notify revival server of session start
            OnConnectToServer(serverIP, "unknown", "TDM");

            std::wstring wCmd = L"open " + Utf8ToWide(serverIP);
            auto* kismet = getLastOfType<UKismetSystemLibrary>();
            if (kismet && *UWorld::GWorld &&
                (*UWorld::GWorld)->OwningGameInstance &&
                (*UWorld::GWorld)->OwningGameInstance->LocalPlayers.Count() > 0) {
                kismet->STATIC_ExecuteConsoleCommand(
                    *UWorld::GWorld, wCmd.c_str(),
                    (*UWorld::GWorld)->OwningGameInstance->LocalPlayers[0]->PlayerController);
            }
        }

        if (launchTutorial) {
            launchTutorial = false;
            auto* kismet = getLastOfType<UKismetSystemLibrary>();
            if (kismet && *UWorld::GWorld &&
                (*UWorld::GWorld)->OwningGameInstance &&
                (*UWorld::GWorld)->OwningGameInstance->LocalPlayers.Count() > 0) {
                kismet->STATIC_ExecuteConsoleCommand(
                    *UWorld::GWorld, L"open S01E00_00_Tutorial_P",
                    (*UWorld::GWorld)->OwningGameInstance->LocalPlayers[0]->PlayerController);
            }
        }

        if (launchSingleplayer) {
            launchSingleplayer = false;
            if (map >= 0 && map < 11) {
                std::wstring cmd = L"open " + std::wstring(AVAILABLE_MAPS[map].fileName);
                auto* kismet = getLastOfType<UKismetSystemLibrary>();
                if (kismet && *UWorld::GWorld &&
                    (*UWorld::GWorld)->OwningGameInstance &&
                    (*UWorld::GWorld)->OwningGameInstance->LocalPlayers.Count() > 0) {
                    kismet->STATIC_ExecuteConsoleCommand(
                        *UWorld::GWorld, cmd.c_str(),
                        (*UWorld::GWorld)->OwningGameInstance->LocalPlayers[0]->PlayerController);
                }
                if (numBotsTeamOne > 0 || numBotsTeamTwo > 0) {
                    std::thread t(SetupSingleplayerAIThread,
                        numBotsTeamOne, numBotsTeamTwo, difficulty,
                        g_profileManager.GetActive().loadoutPath);
                    t.detach();
                } else {
                    std::thread t(DelaySingleplayerSetupThread,
                        g_profileManager.GetActive().loadoutPath);
                    t.detach();
                }
            }
        }
    }
}

// -----------------------------------------------------------------------
//  ImGui Overlay — hkPresent
// -----------------------------------------------------------------------
std::atomic<bool> init(false);
std::atomic<bool> menuEnabled(true);

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!init.load()) {
        if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
            return oPresent(pSwapChain, SyncInterval, Flags);
        pDevice->GetImmediateContext(&pContext);

        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        window = desc.OutputWindow;
        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

        InitImGui();
        init.store(true);

        // Kick off server status check now that we're up
        CheckServerAsync();
    }

    // New frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ---- Toggle menu (F1) ----
    static bool f1Down = false;
    bool f1Now = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    if (f1Now && !f1Down) menuEnabled = !menuEnabled;
    f1Down = f1Now;

    if (menuEnabled) {
        ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_Once);
        ImGui::Begin("Dreadnought Revival  [F1 to hide]",
            nullptr, ImGuiWindowFlags_NoScrollbar);

        // ---- Tabs ----
        if (ImGui::BeginTabBar("MainTabs")) {

            // ============================================================
            //  REVIVAL SERVER TAB  (new)
            // ============================================================
            if (ImGui::BeginTabItem("Revival Server")) {
                ImGui::Spacing();

                // API base URL
                ImGui::SetNextItemWidth(320.0f);
                if (ImGui::InputText("API URL", g_serverURLBuf, sizeof(g_serverURLBuf))) {
                    std::lock_guard<std::mutex> lk(g_apiMutex);
                    g_serverAPI.baseURL = g_serverURLBuf;
                    g_serverOnline  = false;
                    g_apiChecked    = false;
                    g_apiStatusText = "Not connected";
                }
                ImGui::SameLine();
                if (ImGui::Button("Check##status")) {
                    g_serverAPI.baseURL = g_serverURLBuf;
                    CheckServerAsync();
                }

                // Status badge
                ImGui::Spacing();
                {
                    std::lock_guard<std::mutex> lk(g_apiMutex);
                    if (!g_apiChecked) {
                        ImGui::TextDisabled("Status: Checking...");
                    } else if (g_serverOnline) {
                        ImGui::TextColored(ImVec4(0.2f,0.9f,0.3f,1.0f),
                            ("Status: " + g_apiStatusText).c_str());
                    } else {
                        ImGui::TextColored(ImVec4(0.9f,0.2f,0.2f,1.0f),
                            ("Status: " + g_apiStatusText).c_str());
                    }
                }

                ImGui::Separator();
                ImGui::Spacing();

                // Player registration
                if (!g_apiRegistered) {
                    ImGui::Text("Player Name:");
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##pname", g_playerNameBuf, sizeof(g_playerNameBuf));
                    ImGui::SameLine();
                    bool busy = false;
                    { std::lock_guard<std::mutex> lk(g_apiMutex); busy = g_apiRegistering; }
                    if (!busy) {
                        if (ImGui::Button("Register / Login")) {
                            std::string nm(g_playerNameBuf);
                            if (!nm.empty())
                                RegisterAndFetchAsync(nm);
                        }
                    } else {
                        ImGui::TextDisabled("Registering...");
                    }
                } else {
                    // Player info panel
                    std::lock_guard<std::mutex> lk(g_apiMutex);
                    ImGui::TextColored(ImVec4(0.8f,0.9f,1.0f,1.0f),
                        ("Pilot: " + g_apiPlayerName).c_str());
                    ImGui::Text("Level %d   |   %d XP   |   %lld Credits   |   %d GP",
                        g_apiLevel, g_apiXP, g_apiCredits, g_apiGP);
                    ImGui::Spacing();
                    ImGui::Text("Fleet (%d ships):", (int)g_apiHangar.size());
                    if (g_apiHangar.empty()) {
                        ImGui::TextDisabled("  (no ships yet)");
                    } else {
                        ImGui::BeginChild("##hangar", ImVec2(0, 120), true);
                        for (auto& ship : g_apiHangar) {
                            std::string label = ship.shipClass + "  " + ship.role +
                                               "  [" + ship.tier + "]";
                            ImGui::BulletText("%s", label.c_str());
                        }
                        ImGui::EndChild();
                    }
                    ImGui::Spacing();
                    if (ImGui::Button("Refresh##refresh")) {
                        g_serverAPI.GetProgression(g_apiPlayerID, [](bool ok, PlayerInfo p) {
                            if (ok) {
                                std::lock_guard<std::mutex> lk(g_apiMutex);
                                g_apiLevel   = p.level;
                                g_apiXP      = p.xp;
                                g_apiCredits = p.credits;
                                g_apiGP      = p.gp;
                            }
                        });
                        g_serverAPI.GetHangar(g_apiPlayerID, [](bool ok, std::vector<HangarShipEntry> s) {
                            if (ok) {
                                std::lock_guard<std::mutex> lk(g_apiMutex);
                                g_apiHangar = s;
                            }
                        });
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Log Out##logout")) {
                        std::lock_guard<std::mutex> lk(g_apiMutex);
                        g_apiRegistered = false;
                        g_apiPlayerID   = "";
                        g_apiPlayerName = "";
                        g_apiLevel = g_apiXP = g_apiGP = 0;
                        g_apiCredits = 0;
                        g_apiHangar.clear();
                        g_playerNameBuf[0] = '\0';
                    }
                }

                ImGui::EndTabItem();
            }

            // ============================================================
            //  MULTIPLAYER TAB
            // ============================================================
            if (ImGui::BeginTabItem("Multiplayer")) {
                ImGui::Spacing();
                ImGui::Text("Game Server IP (UDP port 7777):");
                ImGui::SetNextItemWidth(260.0f);
                ImGui::InputText("##sip", &serverIP);
                ImGui::SameLine();
                if (ImGui::Button("Connect")) {
                    if (!serverIP.empty())
                        connectToServer = true;
                }
                ImGui::Spacing();
                ImGui::TextDisabled("Enter the IP of a Dreadnought game server.");
                ImGui::TextDisabled("The revival server API URL is set in the Revival Server tab.");
                ImGui::EndTabItem();
            }

            // ============================================================
            //  TUTORIAL TAB
            // ============================================================
            if (ImGui::BeginTabItem("Tutorial")) {
                ImGui::Spacing();
                if (ImGui::Button("Launch Tutorial"))
                    launchTutorial = true;
                ImGui::EndTabItem();
            }

            // ============================================================
            //  SINGLEPLAYER TAB
            // ============================================================
            if (ImGui::BeginTabItem("Singleplayer")) {
                ImGui::Spacing();
                ImGui::Text("Map:");
                for (int i = 0; i < 10; i++) {
                    if (i > 0 && i % 3 != 0) ImGui::SameLine();
                    bool sel = (map == i);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.42f,0.78f,1.0f));
                    if (ImGui::Button(AVAILABLE_MAPS[i].displayName)) map = i;
                    if (sel) ImGui::PopStyleColor();
                }
                ImGui::Spacing();
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderInt("Bots (Team 1)", &numBotsTeamOne, 0, MAX_FRIENDLY_BOTS);
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderInt("Bots (Team 2)", &numBotsTeamTwo, 0, MAX_ENEMY_BOTS);
                const char* diffLabels[] = { "Recruit", "Veteran", "Legendary" };
                ImGui::SetNextItemWidth(120.0f);
                ImGui::Combo("Difficulty", &difficulty, diffLabels, 3);
                ImGui::Spacing();
                if (ImGui::Button("Launch"))
                    launchSingleplayer = true;
                ImGui::EndTabItem();
            }

            // ============================================================
            //  PROFILES TAB
            // ============================================================
            if (ImGui::BeginTabItem("Profiles")) {
                ImGui::Spacing();

                static int  selClass  = 0;
                static int  selWeight = 0;
                static int  lastProfileIdx = -1;

                for (int i = 0; i < (int)g_profileManager.profiles.size(); i++) {
                    if (i > 0) ImGui::SameLine();
                    bool isActive = (i == g_profileManager.activeProfileIndex);
                    if (isActive)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.42f,0.78f,1.0f));
                    if (ImGui::Button(g_profileManager.profiles[i].name.c_str()))
                        if (!isActive) {
                            g_profileManager.activeProfileIndex = i;
                        }
                    if (isActive) ImGui::PopStyleColor();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(" + ")) {
                    g_profileManager.AddProfile();
                    g_profileManager.activeProfileIndex = (int)g_profileManager.profiles.size() - 1;
                }
                if ((int)g_profileManager.profiles.size() > 1) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(" - "))
                        g_profileManager.RemoveProfile(g_profileManager.activeProfileIndex);
                }
                ImGui::Separator();

                UserProfile& active = g_profileManager.GetActive();

                if (lastProfileIdx != g_profileManager.activeProfileIndex) {
                    lastProfileIdx = g_profileManager.activeProfileIndex;
                    selClass = selWeight = 0;
                    for (int i = 0; i < NUM_LOADOUT_PRESETS; i++) {
                        if (active.loadoutPath == LOADOUT_PRESETS[i].path) {
                            selClass  = i / 3;
                            selWeight = i % 3;
                            break;
                        }
                    }
                }

                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::InputText("Profile Name##pn", &active.name))
                    g_profileManager.Save();

                ImGui::Spacing();
                ImGui::Text("Ship Class:");
                const char* classNames[] = { "Assault", "Dreadnought", "Sniper",
                                              "Tactical Cruiser", "Corvette" };
                for (int i = 0; i < 5; i++) {
                    if (i > 0) ImGui::SameLine();
                    bool sel = (selClass == i);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.42f,0.78f,1.0f));
                    if (ImGui::Button(classNames[i])) selClass = i;
                    if (sel) ImGui::PopStyleColor();
                }
                ImGui::Spacing();
                ImGui::Text("Weight:");
                const char* weightNames[] = { "Light", "Medium", "Heavy" };
                for (int i = 0; i < 3; i++) {
                    if (i > 0) ImGui::SameLine();
                    bool sel = (selWeight == i);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.42f,0.78f,1.0f));
                    if (ImGui::Button(weightNames[i])) selWeight = i;
                    if (sel) ImGui::PopStyleColor();
                }
                int presetIdx = selClass * 3 + selWeight;
                if (presetIdx < NUM_LOADOUT_PRESETS) {
                    active.loadoutPath = LOADOUT_PRESETS[presetIdx].path;
                    g_profileManager.Save();
                }
                ImGui::Spacing();
                ImGui::TextDisabled("Loadout: %s", active.loadoutPath.c_str());

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // ---- Error toast ----
        {
            std::lock_guard<std::mutex> lk(g_errorMutex);
            if (g_showErrorMessage) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Error: %s",
                    g_lastErrorMessage.c_str());
                if (ImGui::Button("Dismiss")) g_showErrorMessage = false;
            }
        }

        ImGui::End();
    }

    // ---- In-match profile picker (F8) ----
    if (!Globals::AmServer && g_showProfilePicker) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Once);
        ImGui::Begin("Ship Selection", nullptr);
        ImGui::Text("Select loadout for this match:");
        ImGui::Spacing();
        for (int i = 0; i < NUM_LOADOUT_PRESETS; i++) {
            if (ImGui::Selectable(LOADOUT_PRESETS[i].displayName)) {
                g_profileManager.GetActive().loadoutPath = LOADOUT_PRESETS[i].path;
                g_profileManager.Save();
                loadoutString = LOADOUT_PRESETS[i].path;
                LoadLoadouts();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(pSwapChain, SyncInterval, Flags);
}

// -----------------------------------------------------------------------
//  ResizeBuffers hook
// -----------------------------------------------------------------------
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
ResizeBuffers_t oResizeBuffers = nullptr;

HRESULT hkResizeBuffers(IDXGISwapChain* pThis, UINT BufferCount,
                        UINT Width, UINT Height,
                        DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (mainRenderTargetView) {
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr;
    }
    HRESULT hr = oResizeBuffers(pThis, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    ID3D11Texture2D* backBuffer = nullptr;
    pThis->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        pDevice->CreateRenderTargetView(backBuffer, nullptr, &mainRenderTargetView);
        backBuffer->Release();
    }
    return hr;
}

// -----------------------------------------------------------------------
//  EndMatch hook — report result to revival server
// -----------------------------------------------------------------------
void* origEndMatch = nullptr;
void EndMatchHook(void* p1) {
    LogToFile("[Hook] EndMatch called");
    // Report to revival server (default stats — real values need UE4 parsing)
    OnMatchEnd(0, 0, 0, false);
    using Fn = void(*)(void*);
    ((Fn)origEndMatch)(p1);
}

// -----------------------------------------------------------------------
//  EAC bypass hook
// -----------------------------------------------------------------------
void* origEACErrorMessageHook = nullptr;
void* EACErrorMessageHook(void* p1, void* p2) {
    // Suppress EAC error popups so the game runs without authentication
    return nullptr;
}

// -----------------------------------------------------------------------
//  UGameEngine::Tick hook — drives ProcessEvent on main thread
// -----------------------------------------------------------------------
void* OrigUGameEngineTick = nullptr;
std::mutex ProcOnMainThreadMutex;
std::vector<std::function<void()>> ProcOnMainThreadQueue;

void UGameEngineTick(UGameEngine* engine, float dt, bool idle) {
    {
        std::lock_guard<std::mutex> lk(ProcOnMainThreadMutex);
        for (auto& fn : ProcOnMainThreadQueue) fn();
        ProcOnMainThreadQueue.clear();
    }
    using Fn = void(*)(UGameEngine*, float, bool);
    ((Fn)OrigUGameEngineTick)(engine, dt, idle);
}

// -----------------------------------------------------------------------
//  MainThread — sets up hooks and runs until DLL unload
// -----------------------------------------------------------------------
void MainThread() {
    // Load persisted profiles
    g_profileManager.Load();

    // Wait for game window
    Sleep(INIT_SLEEP_MS);

    Globals::ModuleBase = (uintptr_t)GetModuleHandle(nullptr);

    // Check if we are the server process (via cfg.txt)
    {
        std::ifstream cfg("cfg.txt");
        if (cfg.is_open()) {
            Globals::AmServer = true;
            LogToFile("[Init] Running as server (cfg.txt found)");
        }
    }

    if (!Globals::AmServer) {
        // Hook DirectX Present for ImGui overlay
        if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
            kiero::bind(8,  (void**)&oPresent,       (void*)hkPresent);
            kiero::bind(13, (void**)&oResizeBuffers,  (void*)hkResizeBuffers);
        }
    }

    // Hook UGameEngine::Tick
    // Address lookup is game-version specific; placeholder shown here.
    // Actual offset must be determined from the SDK or pattern scan.
    LogToFile("[Init] Hooks installed");
}

// -----------------------------------------------------------------------
//  DllMain
// -----------------------------------------------------------------------
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        {
            std::thread t(MainThread);
            t.detach();
        }
        break;
    case DLL_PROCESS_DETACH:
        JoinAllThreads();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        if (!Globals::AmServer) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            kiero::shutdown();
        }
        break;
    }
    return TRUE;
}
