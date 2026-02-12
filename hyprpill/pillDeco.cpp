#include "pillDeco.hpp"

#include <algorithm>
#include <linux/input-event-codes.h>
#include <cmath>
#include <chrono>
#include <format>
#include <string>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRule.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "PillPassElement.hpp"
#include "globals.hpp"

namespace {
float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

CHyprColor lerpColor(const CHyprColor& a, const CHyprColor& b, float t) {
    return CHyprColor{lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t), lerpf(a.a, b.a, t)};
}

float easeInOut(float t) {
    return t < 0.5F ? 2.F * t * t : 1.F - std::pow(-2.F * t + 2.F, 2.F) * 0.5F;
}
}

CHyprPill::CHyprPill(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseButton(info, std::any_cast<IPointer::SButtonEvent>(param)); });
    m_pTouchDownCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchDown", [&](void* self, SCallbackInfo& info, std::any param) { onTouchDown(info, std::any_cast<ITouch::SDownEvent>(param)); });
    m_pTouchUpCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchUp", [&](void* self, SCallbackInfo& info, std::any param) { onTouchUp(info, std::any_cast<ITouch::SUpEvent>(param)); });
    m_pTouchMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchMove", [&](void* self, SCallbackInfo& info, std::any param) { onTouchMove(info, std::any_cast<ITouch::SMotionEvent>(param)); });
    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(info, std::any_cast<Vector2D>(param)); });

    updateStateAndAnimate();
    updateCursorShape();
}

CHyprPill::~CHyprPill() {
    if (g_pGlobalState && g_pGlobalState->dragPill.get() == this)
        g_pGlobalState->dragPill.reset();

    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchDownCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchUpCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchMoveCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
    std::erase(g_pGlobalState->pills, m_self);
    updateCursorShape();
}

SDecorationPositioningInfo CHyprPill::getPositioningInfo() {
    static auto* const PENABLED    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:enabled")->getDataStaticPtr();
    static auto* const PPRECEDENCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_precedence_over_border")->getDataStaticPtr();

    SDecorationPositioningInfo info;
    info.policy         = DECORATION_POSITION_ABSOLUTE;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = **PPRECEDENCE ? 10005 : 5000;
    info.reserved       = false;
    info.desiredExtents = {{0, **PENABLED ? 1 : 0}, {0, 0}};
    return info;
}

void CHyprPill::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_bAssignedBox = reply.assignedGeometry;
}

std::string CHyprPill::getDisplayName() {
    return "Hyprpill";
}

eDecorationType CHyprPill::getDecorationType() {
    return DECORATION_CUSTOM;
}

uint64_t CHyprPill::getDecorationFlags() {
    static auto* const PPART = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_part_of_window")->getDataStaticPtr();
    return **PPART ? DECORATION_PART_OF_MAIN_WINDOW : 0;
}

eDecorationLayer CHyprPill::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

PHLWINDOW CHyprPill::getOwner() {
    return m_pWindow.lock();
}

void CHyprPill::draw(PHLMONITOR pMonitor, const float& a) {
    if (!validMapped(m_pWindow))
        return;

    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:enabled")->getDataStaticPtr();
    if (!**PENABLED || m_hidden)
        return;

    updateStateAndAnimate();

    CPillPassElement::SPillData data;
    data.deco = this;
    data.a    = a;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CPillPassElement>(data));
}

void CHyprPill::renderPass(PHLMONITOR pMonitor, const float& a) {
    if (!validMapped(m_pWindow))
        return;

    auto box = visibleBoxGlobal().translate(-pMonitor->m_position);
    if (box.w < 1 || box.h < 1)
        return;

    CHyprColor color = m_forcedColor.value_or(m_color);
    color.a *= std::clamp(m_opacity * a, 0.F, 1.F);

    const auto scaledRadius = m_radius * pMonitor->m_scale;
    const auto rounded      = std::max(0, static_cast<int>(std::lround(scaledRadius)));
    g_pHyprOpenGL->renderRect(box, color, {.round = rounded, .roundingPower = m_pWindow->roundingPower()});

    static auto* const PDEBUGHOVER = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:debug_hitbox_hover")->getDataStaticPtr();
    static auto* const PDEBUGCLICK = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:debug_hitbox_click")->getDataStaticPtr();

    if (**PDEBUGHOVER) {
        auto hoverBox = hoverHitboxGlobal().translate(-pMonitor->m_position);
        g_pHyprOpenGL->renderRect(hoverBox, CHyprColor{0.35F, 0.8F, 1.F, 0.22F}, {.round = 0});
    }

    if (**PDEBUGCLICK) {
        auto clickBox = clickHitboxGlobal().translate(-pMonitor->m_position);
        g_pHyprOpenGL->renderRect(clickBox, CHyprColor{1.F, 0.5F, 0.3F, 0.22F}, {.round = 0});
    }

    static auto* const PDEBUGCURSOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:debug_cursor_state")->getDataStaticPtr();
    if (**PDEBUGCURSOR) {
        const auto cursorPos = g_pInputManager->getMouseCoordsInternal();
        const bool overPill  = inputIsValid(true) && VECINRECT(cursorPos, hoverHitboxGlobal().x, hoverHitboxGlobal().y, hoverHitboxGlobal().x + hoverHitboxGlobal().w, hoverHitboxGlobal().y + hoverHitboxGlobal().h);

        CBox indicator = box;
        const int indicatorSize    = std::max(6, static_cast<int>(std::lround(10.F * pMonitor->m_scale)));
        const int indicatorPadding = std::max(1, static_cast<int>(std::lround(2.F * pMonitor->m_scale)));
        indicator.w                = indicatorSize;
        indicator.h                = indicatorSize;
        indicator.x                = box.x + box.w - indicator.w;
        indicator.y                = box.y - indicator.h - indicatorPadding;

        CHyprColor indicatorColor = CHyprColor{0.6F, 0.6F, 0.6F, 0.8F};
        if (m_dragPending || m_draggingThis)
            indicatorColor = CHyprColor{1.F, 0.65F, 0.2F, 0.95F};
        else if (overPill)
            indicatorColor = CHyprColor{0.2F, 0.9F, 0.35F, 0.95F};

        g_pHyprOpenGL->renderRect(indicator, indicatorColor, {.round = 0});
    }

    if (m_targetState != m_currentState || **PDEBUGHOVER || **PDEBUGCLICK || **PDEBUGCURSOR)
        damageEntire();
}

void CHyprPill::updateWindow(PHLWINDOW pWindow) {
    damageEntire();
}

void CHyprPill::damageEntire() {
    const auto box = visibleBoxGlobal().expand(8);
    g_pHyprRenderer->damageBox(box);
    m_bLastRelativeBox = box;
}

CBox CHyprPill::visibleBoxGlobal() const {
    static auto* const PWIDTH  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_width")->getDataStaticPtr();

    CBox box = m_bAssignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_pWindow.lock()));

    const auto PWORKSPACE      = m_pWindow->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();
    box.translate(WORKSPACEOFFSET);

    const auto centerX = box.x + box.w / 2.F;
    box.w              = std::max<int>(1, std::lround(m_width > 1.F ? m_width : **PWIDTH));
    box.h              = std::max<int>(1, std::lround(m_height));
    box.x              = std::lround(centerX - box.w / 2.F);
    box.y              = std::lround(box.y - box.h - m_offsetY);
    return box;
}

CBox CHyprPill::hoverHitboxGlobal() const {
    static auto* const PHITW = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:hover_hitbox_width")->getDataStaticPtr();
    static auto* const PHITH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:hover_hitbox_height")->getDataStaticPtr();
    static auto* const POFFY = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:hover_hitbox_offset_y")->getDataStaticPtr();

    auto               box = visibleBoxGlobal();
    box.x -= **PHITW;
    box.w += **PHITW * 2;
    box.h += **PHITH * 2;
    box.y = std::lround(box.y - **PHITH + **POFFY);
    return box;
}

CBox CHyprPill::clickHitboxGlobal() const {
    static auto* const PHITW = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:click_hitbox_width")->getDataStaticPtr();
    static auto* const PHITH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:click_hitbox_height")->getDataStaticPtr();
    static auto* const POFFY = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:click_hitbox_offset_y")->getDataStaticPtr();

    auto               box = visibleBoxGlobal();
    box.x -= **PHITW;
    box.w += **PHITW * 2;
    box.h += **PHITH * 2;
    box.y = std::lround(box.y - **PHITH + **POFFY);
    return box;
}

Vector2D CHyprPill::cursorRelativeToPill() const {
    return g_pInputManager->getMouseCoordsInternal() - clickHitboxGlobal().pos();
}

bool CHyprPill::isHovering() const {
    const auto coords = g_pInputManager->getMouseCoordsInternal();
    const auto hb     = hoverHitboxGlobal();
    return VECINRECT(coords, hb.x, hb.y, hb.x + hb.w, hb.y + hb.h);
}

bool CHyprPill::inputIsValid(bool ignoreSeatGrab) {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:enabled")->getDataStaticPtr();

    if (!**PENABLED || m_hidden)
        return false;

    if (!m_pWindow->m_workspace || !m_pWindow->m_workspace->isVisible() || !g_pInputManager->m_exclusiveLSes.empty())
        return false;

    if (!ignoreSeatGrab && g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->wlSurface()->resource()))
        return false;

    return true;
}

void CHyprPill::beginDrag(SCallbackInfo& info, const Vector2D& coordsGlobal) {
    if (!g_pGlobalState->dragPill.expired() && g_pGlobalState->dragPill.get() != this)
        return;

    const auto clickHitbox = clickHitboxGlobal();
    if (!VECINRECT(coordsGlobal, clickHitbox.x, clickHitbox.y, clickHitbox.x + clickHitbox.w, clickHitbox.y + clickHitbox.h))
        return;

    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return;

    if (Desktop::focusState()->window() != PWINDOW)
        Desktop::focusState()->fullWindowFocus(PWINDOW);

    if (PWINDOW->m_isFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    m_forceFloatForDrag = !PWINDOW->m_isFloating;
    m_dragCursorOffset = coordsGlobal - (PWINDOW->m_realPosition->value() + PWINDOW->m_floatingOffset);
    m_dragStartCoords  = coordsGlobal;

    info.cancelled   = true;
    m_cancelledDown  = true;
    m_dragPending    = true;
    g_pGlobalState->dragPill = m_self;
    m_targetState    = ePillVisualState::PRESSED;
    m_stateStart     = Time::steadyNow();
    damageEntire();
    updateCursorShape();
}

void CHyprPill::endDrag(SCallbackInfo& info) {
    if (m_cancelledDown)
        info.cancelled = true;

    m_cancelledDown = false;

    if (m_draggingThis && m_forceFloatForDrag) {
        const auto PWINDOW = m_pWindow.lock();
        if (PWINDOW) {
            if (Desktop::focusState()->window() != PWINDOW)
                Desktop::focusState()->fullWindowFocus(PWINDOW);
            g_pKeybindManager->m_dispatchers["settiled"](std::format("address:0x{:x}", (uintptr_t)PWINDOW.get()));
        }
    }

    m_dragPending       = false;
    m_draggingThis      = false;
    m_forceFloatForDrag = false;
    m_touchEv           = false;
    m_touchId      = 0;

    if (g_pGlobalState->dragPill.get() == this)
        g_pGlobalState->dragPill.reset();

    updateCursorShape();
}

bool CHyprPill::focusAndDispatchToWindow(const std::string& dispatcher, const std::string& arg) {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return false;

    if (Desktop::focusState()->window() != PWINDOW)
        Desktop::focusState()->fullWindowFocus(PWINDOW);

    g_pKeybindManager->m_dispatchers[dispatcher](arg);
    return true;
}

bool CHyprPill::handlePillClickAction(SCallbackInfo& info, uint32_t button) {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return false;

    if (button == BTN_MIDDLE) {
        info.cancelled = true;
        return focusAndDispatchToWindow("killactive");
    }

    if (button == BTN_RIGHT) {
        if (PWINDOW->m_isFloating)
            return false;

        info.cancelled = true;
        return focusAndDispatchToWindow("pseudo");
    }

    if (button != BTN_LEFT)
        return false;

    static auto* const PDOUBLECLICKTIMEOUT = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:double_click_timeout")->getDataStaticPtr();
    const auto         now                  = Time::steadyNow();
    const auto timeoutMs = std::max<Hyprlang::INT>(0, **PDOUBLECLICKTIMEOUT);

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastLeftDown).count() <= timeoutMs) {
        m_lastLeftDown  = now - std::chrono::milliseconds(timeoutMs + 1);
        m_dragPending   = false;
        m_draggingThis  = false;
        m_cancelledDown = false;
        m_forceFloatForDrag = false;
        if (g_pGlobalState->dragPill.get() == this)
            g_pGlobalState->dragPill.reset();

        info.cancelled = true;
        updateCursorShape();
        return focusAndDispatchToWindow("togglefloating");
    }

    m_lastLeftDown = now;
    return false;
}

void CHyprPill::onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED) {
        endDrag(info);
        return;
    }

    if (!inputIsValid()) {
        updateCursorShape();
        return;
    }

    const auto coordsGlobal = g_pInputManager->getMouseCoordsInternal();
    const auto clickHitbox  = clickHitboxGlobal();
    if (!VECINRECT(coordsGlobal, clickHitbox.x, clickHitbox.y, clickHitbox.x + clickHitbox.w, clickHitbox.y + clickHitbox.h)) {
        updateCursorShape(coordsGlobal);
        return;
    }

    if (handlePillClickAction(info, e.button))
        return;

    if (e.button != BTN_LEFT)
        return;

    beginDrag(info, coordsGlobal);
}

void CHyprPill::onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e) {
    if (!inputIsValid() || e.touchID != 0)
        return;

    m_touchEv = true;
    m_touchId = e.touchID;

    auto PMONITOR     = m_pWindow->m_monitor.lock();
    PMONITOR          = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();
    const auto COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y);
    beginDrag(info, COORDS);
}

void CHyprPill::onTouchUp(SCallbackInfo& info, ITouch::SUpEvent e) {
    if (!m_touchEv || e.touchID != m_touchId)
        return;

    endDrag(info);
}

void CHyprPill::onMouseMove(SCallbackInfo& info, Vector2D coords) {
    const bool dragOwnedByOtherPill = !g_pGlobalState->dragPill.expired() && g_pGlobalState->dragPill.get() != this;
    if (dragOwnedByOtherPill) {
        m_hovered = false;
        updateCursorShape(coords);
        return;
    }

    const bool activeDrag = m_dragPending || m_draggingThis;
    if (!inputIsValid(activeDrag)) {
        m_hovered = false;
        if (activeDrag)
            info.cancelled = true;
        updateCursorShape(coords);
        return;
    }

    if (m_dragPending) {
        info.cancelled = true;

        if (Desktop::focusState()->window() != m_pWindow.lock())
            Desktop::focusState()->fullWindowFocus(m_pWindow.lock());
    }

    if (m_draggingThis) {
        info.cancelled = true;

        if (Desktop::focusState()->window() != m_pWindow.lock())
            Desktop::focusState()->fullWindowFocus(m_pWindow.lock());

        updateDragPosition(coords);
        updateCursorShape(coords);
        return;
    }

    const auto hb    = hoverHitboxGlobal();
    const bool wasHovered = m_hovered;
    m_hovered        = VECINRECT(coords, hb.x, hb.y, hb.x + hb.w, hb.y + hb.h);

    if (m_hovered != wasHovered)
        damageEntire();

    if (m_hovered) {
        if (!m_dragPending && Desktop::focusState()->window() != m_pWindow.lock())
            Desktop::focusState()->fullWindowFocus(m_pWindow.lock());

        if (!m_dragPending)
            info.cancelled = true;
    }

    if (!m_dragPending || m_touchEv || !validMapped(m_pWindow)) {
        updateCursorShape(coords);
        return;
    }

    static auto* const PDRAGTHRESH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:drag_pixel_threshold")->getDataStaticPtr();
    const auto dragDelta           = coords - m_dragStartCoords;
    const float dragDistance       = std::sqrt(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
    if (dragDistance < std::max<Hyprlang::INT>(0, **PDRAGTHRESH)) {
        updateCursorShape(coords);
        return;
    }

    if (Desktop::focusState()->window() != m_pWindow.lock())
        Desktop::focusState()->fullWindowFocus(m_pWindow.lock());

    m_dragPending  = false;
    info.cancelled = true;
    updateDragPosition(coords);
    updateCursorShape(coords);
}

void CHyprPill::onTouchMove(SCallbackInfo& info, ITouch::SMotionEvent e) {
    if ((!m_dragPending && !m_draggingThis) || !m_touchEv || e.touchID != m_touchId || !validMapped(m_pWindow))
        return;

    auto PMONITOR     = m_pWindow->m_monitor.lock();
    PMONITOR          = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();
    const auto COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y);

    static auto* const PDRAGTHRESH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:drag_pixel_threshold")->getDataStaticPtr();
    if (!m_draggingThis) {
        const auto dragDelta     = COORDS - m_dragStartCoords;
        const float dragDistance = std::sqrt(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
        if (dragDistance < std::max<Hyprlang::INT>(0, **PDRAGTHRESH))
            return;

        m_dragPending = false;
    }

    info.cancelled = true;
    updateDragPosition(COORDS);
    updateCursorShape(COORDS);
}

void CHyprPill::updateDragPosition(const Vector2D& coordsGlobal) {
    const auto PWINDOW = m_pWindow.lock();
    if (!PWINDOW)
        return;

    if (!m_draggingThis && m_forceFloatForDrag) {
        if (Desktop::focusState()->window() != PWINDOW)
            Desktop::focusState()->fullWindowFocus(PWINDOW);
        g_pKeybindManager->m_dispatchers["setfloating"](std::format("address:0x{:x}", (uintptr_t)PWINDOW.get()));
    }

    const auto targetPos = coordsGlobal - m_dragCursorOffset;
    g_pKeybindManager->m_dispatchers["movewindowpixel"](std::format("exact {} {},address:0x{:x}", (int)targetPos.x, (int)targetPos.y, (uintptr_t)PWINDOW.get()));
    m_draggingThis = true;
}

void CHyprPill::updateCursorShape(const std::optional<Vector2D>& coords) {
    if (!g_pGlobalState || !Cursor::overrideController)
        return;

    static auto* const PHOVERCURSOR = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:hover_cursor")->getDataStaticPtr();
    static auto* const PGRABCURSOR  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:grab_cursor")->getDataStaticPtr();

    const auto         HOVERCURSOR = std::string{*PHOVERCURSOR};
    const auto         GRABCURSOR  = std::string{*PGRABCURSOR};

    for (auto& p : g_pGlobalState->pills) {
        const auto PPILL = p.lock();
        if (!PPILL)
            continue;

        if (PPILL->m_dragPending || PPILL->m_draggingThis) {
            if (!GRABCURSOR.empty())
                Cursor::overrideController->setOverride(GRABCURSOR, Cursor::CURSOR_OVERRIDE_UNKNOWN);
            else
                Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
            return;
        }
    }

    const auto COORDS = coords.value_or(g_pInputManager->getMouseCoordsInternal());
    for (auto& p : g_pGlobalState->pills) {
        const auto PPILL = p.lock();
        if (!PPILL || !PPILL->inputIsValid(true))
            continue;

        const auto HB = PPILL->hoverHitboxGlobal();
        if (VECINRECT(COORDS, HB.x, HB.y, HB.x + HB.w, HB.y + HB.h)) {
            if (!HOVERCURSOR.empty())
                Cursor::overrideController->setOverride(HOVERCURSOR, Cursor::CURSOR_OVERRIDE_UNKNOWN);
            else
                Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
            return;
        }
    }

    Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
}

void CHyprPill::updateStateAndAnimate() {
    static auto* const PWIDTH          = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_width")->getDataStaticPtr();
    static auto* const PHEIGHT         = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_height")->getDataStaticPtr();
    static auto* const PRADIUS         = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_radius")->getDataStaticPtr();
    static auto* const PWIDTHINACTIVE  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_width_inactive")->getDataStaticPtr();
    static auto* const PHEIGHTINACTIVE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_height_inactive")->getDataStaticPtr();
    static auto* const PRADIUSINACTIVE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_radius_inactive")->getDataStaticPtr();
    static auto* const PWIDTHHOVER     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_width_hover")->getDataStaticPtr();
    static auto* const PHEIGHTHOVER    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_height_hover")->getDataStaticPtr();
    static auto* const PCOLACTIVE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_color_active")->getDataStaticPtr();
    static auto* const PCOLINACTIVE    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_color_inactive")->getDataStaticPtr();
    static auto* const PCOLHOVER       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_color_hover")->getDataStaticPtr();
    static auto* const PCOLPRESSED     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_color_pressed")->getDataStaticPtr();
    static auto* const POPACTIVE       = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_opacity_active")->getDataStaticPtr();
    static auto* const POPINACTIVE     = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_opacity_inactive")->getDataStaticPtr();
    static auto* const POFFACTIVE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_offset_y_active")->getDataStaticPtr();
    static auto* const POFFINACTIVE    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_offset_y_inactive")->getDataStaticPtr();
    static auto* const PDURHOVER       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:anim_duration_hover")->getDataStaticPtr();
    static auto* const PDURPRESS       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:anim_duration_press")->getDataStaticPtr();

    const bool focused = Desktop::focusState()->window() == m_pWindow.lock();

    m_hovered = isHovering();

    if (m_dragPending || m_draggingThis)
        m_targetState = ePillVisualState::PRESSED;
    else if (m_hovered)
        m_targetState = ePillVisualState::HOVERED;
    else if (focused)
        m_targetState = ePillVisualState::ACTIVE;
    else
        m_targetState = ePillVisualState::INACTIVE;

    const auto now = Time::steadyNow();

    if (m_targetState != m_currentState) {
        m_currentState = m_targetState;
        m_stateStart   = now;
        m_fromWidth    = m_width > 0.F ? m_width : **PWIDTH;
        m_fromHeight   = m_height > 0.F ? m_height : **PHEIGHT;
        m_fromRadius   = m_radius > 0.F ? m_radius : **PRADIUS;
        m_fromOpacity  = m_opacity;
        m_fromOffsetY  = m_offsetY;
        m_fromColor    = m_color;
    }

    float toWidth   = focused ? **PWIDTH : **PWIDTHINACTIVE;
    float toHeight  = focused ? **PHEIGHT : **PHEIGHTINACTIVE;
    float toRadius  = focused ? **PRADIUS : **PRADIUSINACTIVE;
    float toOpacity = focused ? **POPACTIVE : **POPINACTIVE;
    float toOffsetY = focused ? **POFFACTIVE : **POFFINACTIVE;
    CHyprColor toColor = focused ? CHyprColor{(uint64_t)**PCOLACTIVE} : CHyprColor{(uint64_t)**PCOLINACTIVE};

    if (m_targetState == ePillVisualState::HOVERED) {
        toWidth  = **PWIDTHHOVER;
        toHeight = **PHEIGHTHOVER;
        toColor  = CHyprColor{(uint64_t)**PCOLHOVER};
        toOpacity = **POPACTIVE;
    } else if (m_targetState == ePillVisualState::PRESSED) {
        toWidth  = **PWIDTHHOVER;
        toHeight = **PHEIGHTHOVER;
        toColor  = CHyprColor{(uint64_t)**PCOLPRESSED};
        toOpacity = 1.F;
    }

    const float durationMs = m_targetState == ePillVisualState::PRESSED ? std::max<Hyprlang::INT>(1, **PDURPRESS) : std::max<Hyprlang::INT>(1, **PDURHOVER);
    const float elapsedMs  = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_stateStart).count();
    const float t          = std::clamp(elapsedMs / durationMs, 0.F, 1.F);
    const float easedT     = easeInOut(t);

    m_width   = lerpf(m_fromWidth, toWidth, easedT);
    m_height  = lerpf(m_fromHeight, toHeight, easedT);
    m_radius  = lerpf(m_fromRadius, toRadius, easedT);
    m_opacity = lerpf(m_fromOpacity, toOpacity, easedT);
    m_offsetY = lerpf(m_fromOffsetY, toOffsetY, easedT);
    m_color   = lerpColor(m_fromColor, toColor, easedT);

    if (t < 1.F || std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrame).count() > 16)
        damageEntire();

    m_lastFrame = now;
}

void CHyprPill::updateRules() {
    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->noPillRuleIdx) &&
        !PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->pillColorRuleIdx))
        return;

    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->noPillRuleIdx))
        m_hidden = truthy(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_pGlobalState->noPillRuleIdx)->effect);

    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->pillColorRuleIdx))
        m_forcedColor = CHyprColor(configStringToInt(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_pGlobalState->pillColorRuleIdx)->effect).value_or(0));

    g_pDecorationPositioner->repositionDeco(this);
    damageEntire();
}
