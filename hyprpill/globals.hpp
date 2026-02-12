#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

class CHyprPill;

struct SGlobalState {
    std::vector<WP<CHyprPill>> pills;
    uint32_t                   noPillRuleIdx    = 0;
    uint32_t                   pillColorRuleIdx = 0;
};

inline UP<SGlobalState> g_pGlobalState;
