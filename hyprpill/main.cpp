#define WLR_USE_UNSTABLE

#include <algorithm>
#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "globals.hpp"
#include "pillDeco.hpp"

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onNewWindow(void* self, std::any data) {
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    if (PWINDOW->m_X11DoesntWantBorders)
        return;

    if (std::ranges::any_of(PWINDOW->m_windowDecorations, [](const auto& d) { return d->getDisplayName() == "Hyprpill"; }))
        return;

    auto pill = makeUnique<CHyprPill>(PWINDOW);
    g_pGlobalState->pills.emplace_back(pill);
    pill->m_self = pill;
    pill->updateRules();
    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(pill));
}

static void onUpdateWindowRules(PHLWINDOW window) {
    const auto PILLIT = std::find_if(g_pGlobalState->pills.begin(), g_pGlobalState->pills.end(), [window](const auto& pill) { return pill->getOwner() == window; });

    if (PILLIT == g_pGlobalState->pills.end())
        return;

    (*PILLIT)->updateRules();
    window->updateWindowDecos();
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprpill] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)", CHyprColor{1.0, 0.2, 0.2, 1.0},
                                     5000);
        throw std::runtime_error("[hyprpill] Version mismatch");
    }

    g_pGlobalState                = makeUnique<SGlobalState>();
    g_pGlobalState->noPillRuleIdx = Desktop::Rule::windowEffects()->registerEffect("hyprpill:no_pill");
    g_pGlobalState->pillColorRuleIdx = Desktop::Rule::windowEffects()->registerEffect("hyprpill:pill_color");

    static auto P  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });
    static auto P2 =
        HyprlandAPI::registerCallbackDynamic(PHANDLE, "windowUpdateRules", [&](void* self, SCallbackInfo& info, std::any data) { onUpdateWindowRules(std::any_cast<PHLWINDOW>(data)); });

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_width", Hyprlang::INT{96});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_height", Hyprlang::INT{12});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_radius", Hyprlang::INT{12});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_width_hover", Hyprlang::INT{120});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_height_hover", Hyprlang::INT{14});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_hitbox_width", Hyprlang::INT{10});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_hitbox_height", Hyprlang::INT{8});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_color_active", Hyprlang::INT{*configStringToInt("rgba(99ccffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_color_inactive", Hyprlang::INT{*configStringToInt("rgba(777777cc)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_color_hover", Hyprlang::INT{*configStringToInt("rgba(bbe6ffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_color_pressed", Hyprlang::INT{*configStringToInt("rgba(66b2ffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_opacity_active", Hyprlang::FLOAT{1.F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_opacity_inactive", Hyprlang::FLOAT{0.6F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_offset_y_active", Hyprlang::INT{4});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_offset_y_inactive", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:anim_duration_hover", Hyprlang::INT{140});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:anim_duration_press", Hyprlang::INT{100});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:anim_easing", Hyprlang::STRING{"easeInOut"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_blur", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_part_of_window", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprpill:pill_precedence_over_border", Hyprlang::INT{0});

    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        onNewWindow(nullptr, std::any(w));
    }

    HyprlandAPI::reloadConfig();

    return {"hyprpill", "A plugin to add animated pill grabbers to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_monitors)
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->m_renderPass.removeAllOfType("CPillPassElement");

    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->noPillRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->pillColorRuleIdx);
}
