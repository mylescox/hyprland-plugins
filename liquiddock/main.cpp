#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "dockSurface.hpp"
#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onNewWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    if (auto dock = g_pGlobalState->dock.lock())
        dock->onWindowOpen(PWINDOW);
}

static void onCloseWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    if (auto dock = g_pGlobalState->dock.lock())
        dock->onWindowClose(PWINDOW);
}

static void onWindowFocus(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    if (auto dock = g_pGlobalState->dock.lock())
        dock->onWindowFocus(PWINDOW);
}

Hyprlang::CParseResult onPinnedApp(const char* K, const char* V) {
    std::string            v = V;
    CVarList               vars(v);

    Hyprlang::CParseResult result;

    // liquiddock-pin = appId, command, displayName
    if (vars[0].empty()) {
        result.setError("appId cannot be empty");
        return result;
    }

    SDockItem item;
    item.appId       = vars[0];
    item.command     = vars.size() > 1 ? vars[1] : vars[0];
    item.displayName = vars.size() > 2 ? vars[2] : vars[0];
    item.pinned      = true;

    g_pGlobalState->items.push_back(std::move(item));

    return result;
}

static void onPreConfigReload() {
    // Clear pinned items; they'll be re-added from config
    std::erase_if(g_pGlobalState->items, [](const SDockItem& item) { return item.pinned && !item.running; });
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[LiquidDock] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[liquiddock] Version mismatch");
    }

    g_pGlobalState = makeUnique<SGlobalState>();

    // Register event callbacks
    static auto P  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(self, data); });
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onWindowFocus(self, data); });
    static auto P4 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    // Register configuration values
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:dock_height", Hyprlang::INT{64});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:dock_padding", Hyprlang::INT{8});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:dock_color", Hyprlang::INT{*configStringToInt("rgba(1a1a1aDD)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:dock_radius", Hyprlang::INT{16});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:dock_blur", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:icon_size", Hyprlang::INT{48});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:icon_spacing", Hyprlang::INT{8});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:indicator_color", Hyprlang::INT{*configStringToInt("rgba(ffffffCC)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:auto_hide", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:magnification", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:magnification_scale", Hyprlang::FLOAT{1.5F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:gooey_effect", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:liquiddock:gooey_threshold", Hyprlang::FLOAT{15.F});

    // Register custom keyword for pinning apps
    HyprlandAPI::addConfigKeyword(PHANDLE, "plugin:liquiddock:liquiddock-pin", onPinnedApp, Hyprlang::SHandlerOptions{});

    // Create the dock
    auto dock          = makeShared<CLiquidDock>();
    g_pGlobalState->dock = dock;

    HyprlandAPI::reloadConfig();

    return {"liquiddock", "A dock plugin with gooey SDF rendering and physics-based animations.", "mylescox", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_monitors)
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("CDockPassElement");

    g_pGlobalState.reset();
}
