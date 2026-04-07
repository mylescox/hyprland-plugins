#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/helpers/signal/Signal.hpp>
#include <optional>
#include <chrono>
#include <string>

#define private public
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private

namespace Event {
    struct SCallbackInfo;
}

enum class ePillVisualState {
    INACTIVE = 0,
    ACTIVE,
    HOVERED,
    PRESSED,
};

class CHyprPill : public IHyprWindowDecoration {
  public:
    CHyprPill(PHLWINDOW pWindow);
    virtual ~CHyprPill();

    virtual SDecorationPositioningInfo getPositioningInfo();
    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);
    virtual void                       draw(PHLMONITOR pMonitor, float const& a);
    virtual eDecorationType            getDecorationType();
    virtual void                       updateWindow(PHLWINDOW pWindow);
    virtual void                       damageEntire();
    virtual eDecorationLayer           getDecorationLayer();
    virtual uint64_t                   getDecorationFlags();
    virtual std::string                getDisplayName();

    void                               updateRules();
    PHLWINDOW                          getOwner();

    void                               renderPass(PHLMONITOR pMonitor, float const& a);
    CBox                               visibleBoxGlobal() const;
    CBox                               hoverHitboxGlobal() const;
    CBox                               clickHitboxGlobal() const;

    WP<CHyprPill>                      m_self;

  private:
    void                      onMouseButton(Event::SCallbackInfo& info, IPointer::SButtonEvent e);
    void                      onTouchDown(Event::SCallbackInfo& info, ITouch::SDownEvent e);
    void                      onTouchUp(Event::SCallbackInfo& info, ITouch::SUpEvent e);
    void                      onMouseMove(Event::SCallbackInfo& info, Vector2D coords);
    void                      onTouchMove(Event::SCallbackInfo& info, ITouch::SMotionEvent e);

    void                      beginDrag(Event::SCallbackInfo& info, const Vector2D& coordsGlobal);
    void                      endDrag(Event::SCallbackInfo& info);
    bool                      handlePillClickAction(Event::SCallbackInfo& info, uint32_t button);
    bool                      focusAndDispatchToWindow(const std::string& dispatcher, const std::string& arg = "");
    void                      updateStateAndAnimate();
    void                      updateScoot();
    void                      removeScoot();
    void                      updateDragPosition(const Vector2D& coordsGlobal);
    void                      updateCursorShape(const std::optional<Vector2D>& coords = std::nullopt);
    static CHyprPill*         topmostPillAt(const Vector2D& coordsGlobal, bool clickHitbox, bool ignoreSeatGrab, const CHyprPill* preferred);
    bool                      inputIsValid(bool ignoreSeatGrab = false);
    bool                      inputIsEligibleForRouting(bool ignoreSeatGrab = false) const;
    bool                      ownsInteractionAt(const Vector2D& coordsGlobal, bool clickHitbox, bool ignoreSeatGrab = false) const;
    Vector2D                  cursorRelativeToPill() const;
    bool                      isHovering() const;
    bool                      isHovering(const Vector2D& coordsGlobal) const;

    PHLWINDOWREF              m_pWindow;
    CBox                      m_bAssignedBox;
    CBox                      m_bLastRelativeBox;

    bool                      m_hidden          = false;
    bool                      m_dragPending     = false;
    bool                      m_draggingThis    = false;
    bool                      m_touchEv         = false;
    bool                      m_cancelledDown   = false;
    bool                      m_hovered         = false;
    bool                      m_forceFloatForDrag = false;
    int                       m_touchId         = 0;
    Vector2D                  m_dragCursorOffset;
    Vector2D                  m_dragStartCoords;
    Time::steady_tp           m_lastLeftDown      = Time::steadyNow() - std::chrono::seconds(5);

    ePillVisualState          m_currentState    = ePillVisualState::INACTIVE;
    ePillVisualState          m_targetState     = ePillVisualState::INACTIVE;

    Time::steady_tp           m_stateStart      = Time::steadyNow();
    Time::steady_tp           m_lastFrame       = Time::steadyNow();

    float                     m_width           = 0.F;
    float                     m_height          = 0.F;
    float                     m_radius          = 0.F;
    float                     m_opacity         = 1.F;
    float                     m_offsetY         = 0.F;
    CHyprColor                m_color;

    float                     m_fromWidth       = 0.F;
    float                     m_fromHeight      = 0.F;
    float                     m_fromRadius      = 0.F;
    float                     m_fromOpacity     = 1.F;
    float                     m_fromOffsetY     = 0.F;
    CHyprColor                m_fromColor;

    mutable bool              m_lastFrameDodging     = false;
    mutable int               m_lastFrameResolvedX   = 0;
    mutable int               m_lastFrameResolvedW   = 0;
    mutable float             m_lastFrameDodgeOffset = 0.F;
    mutable int               m_lastFrameDodgeDir    = 0;
    mutable int               m_lastFramePinnedEdge  = 0;

    mutable float             m_scootTarget          = 0.F;
    mutable int               m_scootDir             = 0;
    float                     m_scootOffset          = 0.F;
    float                     m_scootApplied         = 0.F;
    Time::steady_tp           m_scootAnimLastTick    = Time::steadyNow();

    mutable bool              m_geometryAnimInitialized = false;
    mutable float             m_geometryAnimX           = 0.F;
    mutable float             m_geometryAnimW           = 0.F;
    mutable float             m_geometryAnimH           = 0.F;
    mutable Time::steady_tp   m_geometryAnimLastTick    = Time::steadyNow();
    bool                      m_dragGeometryLocked   = false;
    int                       m_dragLockedResolvedX  = 0;
    int                       m_dragLockedResolvedW  = 0;
    int                       m_dragLockedOffsetX    = 0;
    float                     m_dragLockedDodgeOffset = 0.F;
    int                       m_dragLockedDodgeDir    = 0;
    int                       m_dragLockedPinnedEdge  = 0;

    bool                      m_hasLastRenderBox     = false;
    CBox                      m_lastRenderBox;

    CHyprSignalListener       m_pMouseButtonCallback;
    CHyprSignalListener       m_pTouchDownCallback;
    CHyprSignalListener       m_pTouchUpCallback;
    CHyprSignalListener       m_pTouchMoveCallback;
    CHyprSignalListener       m_pMouseMoveCallback;

    std::optional<CHyprColor> m_forcedColor;

    friend class CPillPassElement;
};
