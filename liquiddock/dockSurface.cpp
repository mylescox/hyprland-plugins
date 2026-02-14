#include "dockSurface.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <algorithm>
#include <cmath>

#include "globals.hpp"
#include "DockPassElement.hpp"

// ────────────────────────────────────────────────────────────────────────────
// Constants for SDF rendering geometry
// ────────────────────────────────────────────────────────────────────────────

static constexpr float DOT_RADIUS     = 2.5F;  // Running indicator dot radius
static constexpr float DOT_OFFSET_Y   = 4.F;   // Vertical offset of dots below icons
static constexpr float DOT_MARGIN     = 10.F;   // Extra render area for indicator dots

// ────────────────────────────────────────────────────────────────────────────
// Gooey SDF fragment shader source (embedded)
// ────────────────────────────────────────────────────────────────────────────

static const char* GOOEY_FRAG_SRC = R"glsl(#version 320 es
precision highp float;

in vec2 v_texcoord;
out vec4 fragColor;

#define MAX_ICONS 32

// Icon shapes — their gooey-merged SDF forms the dock surface
uniform vec2  u_iconPositions[MAX_ICONS];
uniform vec2  u_iconSizes[MAX_ICONS];
uniform float u_iconRoundings[MAX_ICONS];
uniform int   u_numIcons;
uniform float u_threshold;

// Running indicator dots
uniform vec2  u_dotPositions[MAX_ICONS];
uniform float u_dotRadii[MAX_ICONS];
uniform int   u_numDots;
uniform vec4  u_dotColor;

// Common
uniform vec4  u_dockColor;
uniform vec2  u_resolution;

float roundRectSDF(vec2 p, vec2 size, float r) {
    vec2 halfSize = size * 0.5;
    vec2 d = abs(p) - (halfSize - r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

float circleSDF(vec2 p, float r) {
    return length(p) - r;
}

void main() {
    vec2 fragPos = v_texcoord * u_resolution;

    // --- Dock surface: gooey-merged icon blobs ---
    float dockAlpha = 0.0;
    if (u_numIcons > 0) {
        float combinedSDF = 1e20;
        for (int i = 0; i < u_numIcons; ++i) {
            if (i >= MAX_ICONS)
                break;
            vec2 p = fragPos - u_iconPositions[i];
            float currentSDF = roundRectSDF(p, u_iconSizes[i], u_iconRoundings[i]);

            // Smooth minimum for gooey blobbing
            float k = u_threshold;
            float h = clamp(0.5 + 0.5 * (currentSDF - combinedSDF) / k, 0.0, 1.0);
            combinedSDF = mix(currentSDF, combinedSDF, h) - k * h * (1.0 - h);
        }
        dockAlpha = smoothstep(1.0, -1.0, combinedSDF);
    }

    // --- Running indicator dots ---
    float dotAlpha = 0.0;
    for (int i = 0; i < u_numDots; ++i) {
        if (i >= MAX_ICONS)
            break;
        float d = circleSDF(fragPos - u_dotPositions[i], u_dotRadii[i]);
        dotAlpha = max(dotAlpha, smoothstep(1.0, -1.0, d));
    }

    // Composite: dock surface, then dots on top
    vec4 color = u_dockColor;
    color.a *= dockAlpha;

    if (dotAlpha > 0.001) {
        vec4 dot = u_dotColor;
        dot.a *= dotAlpha;
        color.rgb = mix(color.rgb, dot.rgb, dot.a);
        color.a = max(color.a, dot.a);
    }

    if (color.a > 0.001) {
        fragColor = color;
    } else {
        discard;
    }
}
)glsl";

static const char* GOOEY_VERT_SRC = R"glsl(#version 320 es
precision highp float;

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texcoord;

uniform vec2 u_topLeft;
uniform vec2 u_fullSize;
uniform vec2 u_monitorSize;

out vec2 v_texcoord;

void main() {
    v_texcoord = a_texcoord;
    // Map quad from (0,0)-(1,1) to monitor NDC coordinates
    vec2 pos = u_topLeft + a_position * u_fullSize;
    vec2 ndc = (pos / u_monitorSize) * 2.0 - 1.0;
    ndc.y = -ndc.y; // flip Y for GL
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)glsl";

// ────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ────────────────────────────────────────────────────────────────────────────

CLiquidDock::CLiquidDock() {
    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseButton(info, std::any_cast<IPointer::SButtonEvent>(param)); });
    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });

    initShader();
    rebuildDockItems();
    layoutIcons();
    damageEntire();
}

CLiquidDock::~CLiquidDock() {
    destroyShader();
    m_pMouseButtonCallback.reset();
    m_pMouseMoveCallback.reset();
}

// ────────────────────────────────────────────────────────────────────────────
// Shader management
// ────────────────────────────────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Log::logger->log(Log::ERR, "[LiquidDock] Shader compile error: {}", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void CLiquidDock::initShader() {
    GLuint vert = compileShader(GL_VERTEX_SHADER, GOOEY_VERT_SRC);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, GOOEY_FRAG_SRC);

    if (!vert || !frag) {
        if (vert)
            glDeleteShader(vert);
        if (frag)
            glDeleteShader(frag);
        return;
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vert);
    glAttachShader(m_shaderProgram, frag);
    glLinkProgram(m_shaderProgram);

    GLint linked = 0;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(m_shaderProgram, sizeof(log), nullptr, log);
        Log::logger->log(Log::ERR, "[LiquidDock] Shader link error: {}", log);
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

void CLiquidDock::destroyShader() {
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Dock geometry
// ────────────────────────────────────────────────────────────────────────────

PHLMONITOR CLiquidDock::getTargetMonitor() const {
    static auto* const PMONITORNAME = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:monitor")->getDataStaticPtr();

    const std::string monName = *PMONITORNAME;
    if (!monName.empty()) {
        for (auto& m : g_pCompositor->m_monitors) {
            if (m && m->m_name == monName)
                return m;
        }
    }

    // Default: use the first monitor (primary)
    if (!g_pCompositor->m_monitors.empty())
        return g_pCompositor->m_monitors.front();

    return nullptr;
}

CBox CLiquidDock::dockBoxGlobal() const {
    static auto* const PHEIGHT  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:dock_height")->getDataStaticPtr();
    static auto* const PPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:dock_padding")->getDataStaticPtr();
    static auto* const PICONSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_size")->getDataStaticPtr();
    static auto* const PSPACING  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_spacing")->getDataStaticPtr();

    const auto PMONITOR = getTargetMonitor();
    if (!PMONITOR)
        return {};

    const int iconSize = **PICONSIZE;
    const int spacing  = **PSPACING;
    const int padding  = **PPADDING;
    const int numIcons = std::max((int)g_pGlobalState->items.size(), 1);

    const float dockWidth  = numIcons * iconSize + (numIcons - 1) * spacing + 2 * padding;
    const float dockHeight = **PHEIGHT;

    const float monW = PMONITOR->m_size.x;
    const float monH = PMONITOR->m_size.y;

    const float x = PMONITOR->m_position.x + (monW - dockWidth) * 0.5F;
    const float y = PMONITOR->m_position.y + monH - dockHeight - 8.F; // 8px margin from bottom

    return CBox{x, y, dockWidth, dockHeight};
}

// ────────────────────────────────────────────────────────────────────────────
// Dock item management
// ────────────────────────────────────────────────────────────────────────────

void CLiquidDock::rebuildDockItems() {
    // Scan running windows and update items
    auto& items = g_pGlobalState->items;

    // Mark all non-pinned items as not running
    for (auto& item : items) {
        if (!item.pinned)
            item.running = false;
        item.focused = false;
    }

    // Track running windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        const std::string appId = w->m_initialClass;
        if (appId.empty())
            continue;

        // Check if this app already exists in the dock
        auto it = std::find_if(items.begin(), items.end(), [&appId](const SDockItem& item) { return item.appId == appId; });

        if (it != items.end()) {
            it->running = true;
            if (w == Desktop::focusState()->window())
                it->focused = true;
        } else {
            // Add new running app to dock
            SDockItem newItem;
            newItem.appId       = appId;
            newItem.displayName = w->m_title.empty() ? appId : w->m_title;
            newItem.running     = true;
            newItem.focused     = (w == Desktop::focusState()->window());
            items.push_back(std::move(newItem));
        }
    }

    // Remove non-pinned, non-running items
    std::erase_if(items, [](const SDockItem& item) { return !item.pinned && !item.running; });
}

void CLiquidDock::layoutIcons() {
    static auto* const PICONSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_size")->getDataStaticPtr();
    static auto* const PSPACING  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_spacing")->getDataStaticPtr();
    static auto* const PPADDING  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:dock_padding")->getDataStaticPtr();

    const CBox   box      = dockBoxGlobal();
    const int    iconSize = **PICONSIZE;
    const int    spacing  = **PSPACING;
    const int    padding  = **PPADDING;

    for (size_t i = 0; i < g_pGlobalState->items.size(); ++i) {
        const float x = box.x + padding + i * (iconSize + spacing) + iconSize * 0.5F;
        const float y = box.y + box.h * 0.5F;

        // Set target positions; Hyprland's animation system interpolates
        auto& item = g_pGlobalState->items[i];
        if (item.position)
            *item.position = Vector2D{x, y};
        if (item.size)
            *item.size = Vector2D{(float)iconSize, (float)iconSize};
        if (item.scale)
            *item.scale = 1.F;
        if (item.alpha)
            *item.alpha = 1.F;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Magnification effect (macOS-style)
// ────────────────────────────────────────────────────────────────────────────

float CLiquidDock::getMagnificationScale(int iconIndex, float cursorX) const {
    static auto* const PMAGNIFY  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:magnification")->getDataStaticPtr();
    static auto* const PMAGNIFYSCALE = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:magnification_scale")->getDataStaticPtr();

    if (!**PMAGNIFY || cursorX < 0)
        return 1.F;

    const auto& items = g_pGlobalState->items;
    if (iconIndex < 0 || iconIndex >= (int)items.size())
        return 1.F;

    const auto& item = items[iconIndex];
    if (!item.position)
        return 1.F;

    const float iconCenterX = item.position->value().x;
    const float dist        = std::abs(cursorX - iconCenterX);
    const float maxDist     = 150.F; // range of magnification effect in px

    if (dist > maxDist)
        return 1.F;

    const float factor = 1.F - (dist / maxDist);
    const float maxScale = **PMAGNIFYSCALE;
    return 1.F + (maxScale - 1.F) * factor * factor; // quadratic falloff
}

// ────────────────────────────────────────────────────────────────────────────
// Window events
// ────────────────────────────────────────────────────────────────────────────

void CLiquidDock::onWindowOpen(PHLWINDOW window) {
    m_bItemsDirty = true;
    damageEntire();
}

void CLiquidDock::onWindowClose(PHLWINDOW window) {
    m_bItemsDirty = true;
    damageEntire();
}

void CLiquidDock::onWindowFocus(PHLWINDOW window) {
    m_bItemsDirty = true;
    damageEntire();
}

// ────────────────────────────────────────────────────────────────────────────
// Input handling
// ────────────────────────────────────────────────────────────────────────────

int CLiquidDock::hitTestIcon(Vector2D coords) const {
    static auto* const PICONSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_size")->getDataStaticPtr();

    const int    iconSize = **PICONSIZE;
    const float  halfSize = iconSize * 0.5F;

    for (size_t i = 0; i < g_pGlobalState->items.size(); ++i) {
        const auto& item = g_pGlobalState->items[i];
        if (!item.position)
            continue;

        const auto  pos  = item.position->value();
        const float scale = getMagnificationScale(i, m_fCursorX);
        const float scaledHalf = halfSize * scale;

        if (coords.x >= pos.x - scaledHalf && coords.x <= pos.x + scaledHalf && coords.y >= pos.y - scaledHalf && coords.y <= pos.y + scaledHalf)
            return i;
    }
    return -1;
}

void CLiquidDock::onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e) {
    const auto coords = g_pInputManager->getMouseCoordsInternal();
    const auto box    = dockBoxGlobal();

    if (!box.containsPoint(coords))
        return;

    if (e.state == WL_POINTER_BUTTON_STATE_PRESSED && e.button == BTN_LEFT) {
        const int idx = hitTestIcon(coords);
        if (idx >= 0) {
            info.cancelled = true;
            launchApp(g_pGlobalState->items[idx]);
        }
    } else if (e.state == WL_POINTER_BUTTON_STATE_PRESSED && e.button == BTN_RIGHT) {
        const int idx = hitTestIcon(coords);
        if (idx >= 0) {
            info.cancelled = true;
            auto& item = g_pGlobalState->items[idx];
            if (item.pinned) {
                item.pinned = false;
                if (!item.running)
                    m_bItemsDirty = true;
            } else {
                item.pinned = true;
            }
            damageEntire();
        }
    }
}

void CLiquidDock::onMouseMove(Vector2D coords) {
    const auto box = dockBoxGlobal();

    m_fCursorX = coords.x;
    m_fCursorY = coords.y;

    bool wasHovered          = g_pGlobalState->dockHovered;
    g_pGlobalState->dockHovered = box.containsPoint(coords);

    if (wasHovered != g_pGlobalState->dockHovered)
        damageEntire();

    // Update magnification when cursor is over dock
    if (g_pGlobalState->dockHovered)
        damageEntire();
}

void CLiquidDock::launchApp(const SDockItem& item) {
    if (item.running) {
        // Focus existing window
        for (auto& w : g_pCompositor->m_windows) {
            if (w->isHidden() || !w->m_isMapped)
                continue;
            if (w->m_initialClass == item.appId) {
                Desktop::focusState()->fullWindowFocus(w);
                return;
            }
        }
    }

    // Launch via command if available
    if (!item.command.empty()) {
        HyprlandAPI::invokeHyprctlCommand("dispatch", "exec " + item.command);
    } else if (!item.appId.empty()) {
        HyprlandAPI::invokeHyprctlCommand("dispatch", "exec " + item.appId);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Rendering
// ────────────────────────────────────────────────────────────────────────────

void CLiquidDock::damageEntire() {
    const auto box = dockBoxGlobal();
    if (box.empty())
        return;

    // Expand to cover indicator dots below the dock
    g_pHyprRenderer->damageBox(box.expand(DOT_MARGIN));
}

void CLiquidDock::renderDockSDF(PHLMONITOR monitor, float alpha) {
    static auto* const PCOLOR          = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:dock_color")->getDataStaticPtr();
    static auto* const PICONSIZE       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_size")->getDataStaticPtr();
    static auto* const PTHRESHOLD      = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:gooey_threshold")->getDataStaticPtr();
    static auto* const PINDICATORCOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:indicator_color")->getDataStaticPtr();

    if (!m_shaderProgram)
        return;

    const int numIcons = std::min((int)g_pGlobalState->items.size(), 32);
    if (numIcons == 0)
        return;

    const CBox globalBox = dockBoxGlobal();
    if (globalBox.empty())
        return;

    // The SDF render area extends beyond the dock box to cover indicator dots below
    const CBox  renderBox = globalBox.copy().expand(DOT_MARGIN);
    const CBox  localRenderBox = renderBox.copy().translate(-monitor->m_position);

    const float renderW = renderBox.w;
    const float renderH = renderBox.h;
    const int   iconSize = **PICONSIZE;

    glUseProgram(m_shaderProgram);
    GLint loc;

    // Vertex shader uniforms: map quad to monitor-local coordinates
    loc = glGetUniformLocation(m_shaderProgram, "u_topLeft");
    glUniform2f(loc, localRenderBox.x, localRenderBox.y);
    loc = glGetUniformLocation(m_shaderProgram, "u_fullSize");
    glUniform2f(loc, localRenderBox.w, localRenderBox.h);
    loc = glGetUniformLocation(m_shaderProgram, "u_monitorSize");
    glUniform2f(loc, monitor->m_size.x, monitor->m_size.y);

    // Fragment shader uniforms
    loc = glGetUniformLocation(m_shaderProgram, "u_resolution");
    glUniform2f(loc, renderW, renderH);

    CHyprColor dockColor = **PCOLOR;
    loc = glGetUniformLocation(m_shaderProgram, "u_dockColor");
    glUniform4f(loc, dockColor.r, dockColor.g, dockColor.b, dockColor.a * alpha);

    // Icon blobs — these merge together via smooth-min to form the dock
    loc = glGetUniformLocation(m_shaderProgram, "u_numIcons");
    glUniform1i(loc, numIcons);
    loc = glGetUniformLocation(m_shaderProgram, "u_threshold");
    glUniform1f(loc, **PTHRESHOLD);

    for (int i = 0; i < numIcons; ++i) {
        const auto& item = g_pGlobalState->items[i];
        if (!item.position || !item.size)
            continue;

        // Icon positions relative to the render area
        const auto globalPos = item.position->value();
        const float posX = globalPos.x - renderBox.x;
        const float posY = globalPos.y - renderBox.y;

        const float scale = getMagnificationScale(i, m_fCursorX);
        const float sz = iconSize * scale;

        char buf[128];
        snprintf(buf, sizeof(buf), "u_iconPositions[%d]", i);
        loc = glGetUniformLocation(m_shaderProgram, buf);
        glUniform2f(loc, posX, posY);

        snprintf(buf, sizeof(buf), "u_iconSizes[%d]", i);
        loc = glGetUniformLocation(m_shaderProgram, buf);
        glUniform2f(loc, sz, sz);

        snprintf(buf, sizeof(buf), "u_iconRoundings[%d]", i);
        loc = glGetUniformLocation(m_shaderProgram, buf);
        glUniform1f(loc, sz * 0.2F);
    }

    // Running indicator dots
    int numDots = 0;
    for (int i = 0; i < numIcons; ++i) {
        const auto& item = g_pGlobalState->items[i];
        if (!item.running || !item.position)
            continue;

        const auto globalPos = item.position->value();
        const float scale = getMagnificationScale(i, m_fCursorX);
        const float half = iconSize * scale * 0.5F;

        const float dotX = globalPos.x - renderBox.x;
        const float dotY = globalPos.y + half + DOT_OFFSET_Y - renderBox.y;

        char buf[128];
        snprintf(buf, sizeof(buf), "u_dotPositions[%d]", numDots);
        loc = glGetUniformLocation(m_shaderProgram, buf);
        glUniform2f(loc, dotX, dotY);

        snprintf(buf, sizeof(buf), "u_dotRadii[%d]", numDots);
        loc = glGetUniformLocation(m_shaderProgram, buf);
        glUniform1f(loc, DOT_RADIUS);

        numDots++;
    }
    loc = glGetUniformLocation(m_shaderProgram, "u_numDots");
    glUniform1i(loc, numDots);

    CHyprColor indicatorColor = **PINDICATORCOLOR;
    loc = glGetUniformLocation(m_shaderProgram, "u_dotColor");
    glUniform4f(loc, indicatorColor.r, indicatorColor.g, indicatorColor.b, indicatorColor.a * alpha);

    // Draw quad mapped to the dock render area
    // clang-format off
    float vertices[] = {
        0.F, 0.F,  0.F, 0.F,
        1.F, 0.F,  1.F, 0.F,
        1.F, 1.F,  1.F, 1.F,
        0.F, 1.F,  0.F, 1.F,
    };
    // clang-format on

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glUseProgram(0);
}

void CLiquidDock::renderDockIcons(PHLMONITOR monitor, float alpha) {
    static auto* const PICONSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:icon_size")->getDataStaticPtr();

    const int iconSize = **PICONSIZE;

    for (size_t i = 0; i < g_pGlobalState->items.size(); ++i) {
        const auto& item = g_pGlobalState->items[i];
        if (!item.position)
            continue;

        const auto  pos   = item.position->value();
        const float scale = getMagnificationScale(i, m_fCursorX);
        const float sz    = iconSize * scale;
        const float half  = sz * 0.5F;

        CBox iconBox = {pos.x - half - monitor->m_position.x, pos.y - half - monitor->m_position.y, sz, sz};

        // Render icon texture if available, otherwise render a placeholder rounded rect
        if (item.iconTex && item.iconTex->m_texID) {
            g_pHyprOpenGL->renderTexture(item.iconTex, iconBox, {.a = alpha});
        } else {
            // Placeholder: colored rounded square with first letter
            CHyprColor iconColor = item.focused ? CHyprColor{0.4F, 0.6F, 1.F, alpha} : CHyprColor{0.5F, 0.5F, 0.5F, alpha};
            g_pHyprOpenGL->renderRect(iconBox, iconColor, {.round = static_cast<int>(sz * 0.2F)});
        }
    }
}

void CLiquidDock::renderPass(PHLMONITOR monitor, float const& a) {
    static auto* const PENABLED  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:enabled")->getDataStaticPtr();
    static auto* const PAUTOHIDE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:auto_hide")->getDataStaticPtr();

    if (!**PENABLED)
        return;

    // Only render on the target monitor
    const auto targetMonitor = getTargetMonitor();
    if (!targetMonitor || monitor != targetMonitor)
        return;

    // Auto-hide logic
    if (**PAUTOHIDE && !g_pGlobalState->dockHovered) {
        if (!m_bAutoHideActive) {
            m_bAutoHideActive = true;
            m_lastHoverTime   = Time::steadyNow();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Time::steadyNow() - m_lastHoverTime).count();
        if (elapsed > 1500) // Hide after 1.5s without hover
            return;
    } else {
        m_bAutoHideActive = false;
    }

    // Update dock items only when state has changed
    if (m_bItemsDirty) {
        rebuildDockItems();
        layoutIcons();
        m_bItemsDirty = false;
    }

    // Render layers: SDF pass draws dock background, gooey blobs, and indicator dots
    renderDockSDF(monitor, a);
    // Icon textures/placeholders are rendered separately on top
    renderDockIcons(monitor, a);

    // Request damage so the dock area is redrawn next frame
    damageEntire();
}
