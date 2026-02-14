#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>

inline HANDLE PHANDLE = nullptr;

// Represents a single item in the dock (pinned app, running app, or custom button)
struct SDockItem {
    std::string appId;       // application ID / desktop entry name
    std::string command;     // command to execute on click
    std::string iconPath;    // path to icon file
    std::string displayName; // display name for tooltip
    bool        pinned  = false;
    bool        running = false;
    bool        focused = false;
    SP<CTexture> iconTex;

    // Animated properties for physics-based animation
    PHLANIMVAR<Vector2D> position;
    PHLANIMVAR<Vector2D> size;
    PHLANIMVAR<float>    scale;
    PHLANIMVAR<float>    alpha;
};

class CLiquidDock;

struct SGlobalState {
    WP<CLiquidDock>         dock;
    std::vector<SDockItem>  items;
    int                     dragIndex     = -1;
    bool                    dockVisible   = true;
    bool                    dockHovered   = false;
};

inline UP<SGlobalState> g_pGlobalState;
