// DWD Windows storage policy; no gameplay or mod bytecode changes.
#pragma once
#include "Configuration.h"
#include "utils.h"
#include <cctype>
#include <algorithm>
#include <string>
#include <vector>

inline std::string dwd_path_component(const std::string& value) {
    std::string result;
    const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = value[i];
        const bool safe = std::isalnum(c) || c == '-' || c == '_'
            || ((c == '.' || c == ' ') && i + 1 != value.size() && value != "." && value != "..");
        if (safe) result += static_cast<char>(c);
        else { result += '~'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result.empty() ? "game" : result;
}

// Read-only preflight: this runs before probing/creating anything under root.
inline void dwd_preflight_config(Configuration& cfg, const std::string& root) {
    add_system_path("<HOME>", root);
    add_system_path("<CONFIG>", root);
    add_system_path("<GAMEHOME>", root);
    add_system_path("<DATA>", root + "/data");
    for (const auto& game : cfg.listkeys("config/disk/game", false)) {
        for (const char* field : {"path", "static_path"}) {
            std::string value;
            cfg.value("config/disk/game/" + game + "/" + field, value, "");
            if (!value.empty()) dwd_validate_asset_path(root, value);
        }
    }
}

inline void dwd_configure_paths(Configuration& cfg) {
    if (!dwd_portable_runtime() || dwd_developer_run()) return;
    bool changed = false;
    auto set = [&](const std::string& key, const std::string& value) {
        std::string old;
        cfg.value(key, old, "");
        if (old != value) { cfg.set(key, value, false); changed = true; }
    };
    set("config/disk/data_path", "<HOME>/data");
    set("config/disk/music_path", "<HOME>/data/music");
    auto games = cfg.listkeys("config/disk/game", false);
    for (const char* name : {"blackgate", "forgeofvirtue", "serpentisle", "silverseed", "serpentbeta", "demo"}) {
        if (std::find(games.begin(), games.end(), name) == games.end()) games.emplace_back(name);
    }
    for (const auto& game : games) {
        const auto key = "config/disk/game/" + game;
        const auto component = dwd_path_component(game);
        set(key + "/mods", "<HOME>/mods/" + component);
        set(key + "/patch", "<HOME>/patches/" + component);
        set(key + "/source", "<HOME>/patches/" + component + "/source");
        set(key + "/savegame_path", "<HOME>/saves/" + component);
        set(key + "/gamedat_path", "<HOME>/gamedat/" + component);
        for (const auto& directory : {"mods/", "patches/", "saves/", "gamedat/"}) {
            U7mkdir(dwd_owned_path(std::string(directory) + component).c_str(), 0755, true);
        }
    }
    if (changed) cfg.write_back();
}
