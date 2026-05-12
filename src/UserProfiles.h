#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------
//  User Profile: stores a display name and a precast loadout path
// ---------------------------------------------------------------
struct UserProfile {
	std::string name;
	std::string loadoutPath;

	UserProfile()
		: name("Default")
		, loadoutPath("/Game/Generic/Loadouts/Precast/T5/VH_AssaultLight_PrecastLoadout_T5_BP")
	{}

	UserProfile(const std::string& n, const std::string& lp)
		: name(n), loadoutPath(lp)
	{}
};

// ---------------------------------------------------------------
//  Known precast loadout presets (display name + asset path)
//  Paths follow the pattern used in the existing codebase.
//  Users can always enter a custom path manually.
// ---------------------------------------------------------------
struct LoadoutPreset {
	const char* displayName;
	const char* path;
};

static const LoadoutPreset LOADOUT_PRESETS[] = {
	// Assault Frigate  (Light uses legacy naming; Medium/Heavy use T5-prefixed naming)
	{ "Assault Frigate  - T5 Light",  "/Game/Generic/Loadouts/Precast/T5/VH_AssaultLight_PrecastLoadout_T5_BP" },
	{ "Assault Frigate  - T5 Medium", "/Game/Generic/Loadouts/Precast/T5/VH_AssaultMedium_T5_PrecastLoadout_BP" },
	{ "Assault Frigate  - T5 Heavy",  "/Game/Generic/Loadouts/Precast/T5/VH_AssaultHeavy_T5_PrecastLoadout_BP" },
	// Dreadnought
	{ "Dreadnought      - T5 Light",  "/Game/Generic/Loadouts/Precast/T5/VH_DreadnoughtLight_T5_PrecastLoadout_BP" },
	{ "Dreadnought      - T5 Medium", "/Game/Generic/Loadouts/Precast/T5/VH_DreadnoughtMedium_T5_PrecastLoadout_BP" },
	{ "Dreadnought      - T5 Heavy",  "/Game/Generic/Loadouts/Precast/T5/VH_DreadnoughtHeavy_T5_PrecastLoadout_BP" },
	// Sniper / Artillery
	{ "Sniper           - T5 Light",  "/Game/Generic/Loadouts/Precast/T5/VH_SniperLight_T5_PrecastLoadout_BP" },
	{ "Sniper           - T5 Medium", "/Game/Generic/Loadouts/Precast/T5/VH_SniperMedium_T5_PrecastLoadout_BP" },
	{ "Sniper           - T5 Heavy",  "/Game/Generic/Loadouts/Precast/T5/VH_SniperHeavy_T5_PrecastLoadout_BP" },
	// Support / Tactical Cruiser
	{ "Tactical Cruiser - T5 Light",  "/Game/Generic/Loadouts/Precast/T5/VH_SupportLight_T5_PrecastLoadout_BP" },
	{ "Tactical Cruiser - T5 Medium", "/Game/Generic/Loadouts/Precast/T5/VH_SupportMedium_T5_PrecastLoadout_BP" },
	{ "Tactical Cruiser - T5 Heavy",  "/Game/Generic/Loadouts/Precast/T5/VH_SupportHeavy_T5_PrecastLoadout_BP" },
	// Corvette
	{ "Corvette         - T5 Light",  "/Game/Generic/Loadouts/Precast/T5/VH_ScoutLight_T5_PrecastLoadout_BP" },
	{ "Corvette         - T5 Medium", "/Game/Generic/Loadouts/Precast/T5/VH_ScoutMedium_T5_PrecastLoadout_BP" },
	{ "Corvette         - T5 Heavy",  "/Game/Generic/Loadouts/Precast/T5/VH_ScoutHeavy_T5_PrecastLoadout_BP" },
};

static const int NUM_LOADOUT_PRESETS = static_cast<int>(sizeof(LOADOUT_PRESETS) / sizeof(LOADOUT_PRESETS[0]));

// ---------------------------------------------------------------
//  ProfileManager: persist and manage a list of UserProfiles
//
//  File format (profiles.ini):
//    active=<index>
//    name=<profile name>
//    loadout=<loadout path>
//    name=...
//    loadout=...
// ---------------------------------------------------------------
class ProfileManager {
public:
	std::vector<UserProfile> profiles;
	int activeProfileIndex = 0;

	static constexpr const char* PROFILES_FILE = "profiles.ini";

	ProfileManager() {
		profiles.emplace_back(); // Default profile
	}

	UserProfile& GetActive() {
		if (activeProfileIndex < 0 || activeProfileIndex >= static_cast<int>(profiles.size()))
			activeProfileIndex = 0;
		return profiles[activeProfileIndex];
	}

	const UserProfile& GetActive() const {
		int idx = (activeProfileIndex >= 0 && activeProfileIndex < static_cast<int>(profiles.size()))
			? activeProfileIndex : 0;
		return profiles[idx];
	}

	void AddProfile(const std::string& name = "New Profile") {
		profiles.emplace_back(name, GetActive().loadoutPath);
	}

	void RemoveProfile(int index) {
		if (static_cast<int>(profiles.size()) <= 1) return;
		profiles.erase(profiles.begin() + index);
		if (activeProfileIndex >= static_cast<int>(profiles.size()))
			activeProfileIndex = static_cast<int>(profiles.size()) - 1;
	}

	void Load() {
		std::ifstream file(PROFILES_FILE);
		if (!file.is_open()) return;

		std::vector<UserProfile> loaded;
		int activeIdx = 0;
		std::string line, pendingName, pendingPath;

		auto flush = [&]() {
			if (!pendingName.empty() && !pendingPath.empty()) {
				loaded.emplace_back(pendingName, pendingPath);
				pendingName.clear();
				pendingPath.clear();
			}
		};

		while (std::getline(file, line)) {
			if (line.empty()) continue;
			if (line.rfind("active=", 0) == 0) {
				try { activeIdx = std::stoi(line.substr(7)); } catch (...) {}
			} else if (line.rfind("name=", 0) == 0) {
				flush();
				pendingName = line.substr(5);
			} else if (line.rfind("loadout=", 0) == 0) {
				pendingPath = line.substr(8);
			}
		}
		flush();

		if (!loaded.empty()) {
			profiles = std::move(loaded);
			activeProfileIndex = (activeIdx >= 0 && activeIdx < static_cast<int>(profiles.size()))
				? activeIdx : 0;
		}
	}

	void Save() const {
		std::ofstream file(PROFILES_FILE);
		if (!file.is_open()) return;
		file << "active=" << activeProfileIndex << "\n";
		for (const auto& p : profiles) {
			file << "name=" << p.name << "\n";
			file << "loadout=" << p.loadoutPath << "\n";
		}
	}
};
