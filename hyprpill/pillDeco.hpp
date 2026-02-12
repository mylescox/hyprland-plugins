#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/devices/ITouch.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <optional>
#include <chrono>
#include <string>

#define private public
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private

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
    void                      onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e);
    void                      onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e);
    void                      onTouchUp(SCallbackInfo& info, ITouch::SUpEvent e);
    void                      onMouseMove(SCallbackInfo& info, Vector2D coords);
    void                      onTouchMove(SCallbackInfo& info, ITouch::SMotionEvent e);

    void                      beginDrag(SCallbackInfo& info, const Vector2D& coordsGlobal);
    void                      endDrag(SCallbackInfo& info);
    bool                      handlePillClickAction(SCallbackInfo& info, uint32_t button);
    bool                      focusAndDispatchToWindow(const std::string& dispatcher, const std::string& arg = "");
    void                      updateStateAndAnimate();
    void                      updateDragPosition(const Vector2D& coordsGlobal);
    void                      updateCursorShape(const std::optional<Vector2D>& coords = std::nullopt);
    bool                      inputIsValid(bool ignoreSeatGrab = false);
    Vector2D                  cursorRelativeToPill() const;
    bool                      isHovering() const;

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
    bool                      m_dragGeometryLocked   = false;
    int                       m_dragLockedResolvedX  = 0;
    int                       m_dragLockedResolvedW  = 0;
    int                       m_dragLockedOffsetX    = 0;

    bool                      m_hasLastRenderBox     = false;
    CBox                      m_lastRenderBox;

    SP<HOOK_CALLBACK_FN>      m_pMouseButtonCallback;
    SP<HOOK_CALLBACK_FN>      m_pTouchDownCallback;
    SP<HOOK_CALLBACK_FN>      m_pTouchUpCallback;
    SP<HOOK_CALLBACK_FN>      m_pTouchMoveCallback;
    SP<HOOK_CALLBACK_FN>      m_pMouseMoveCallback;

    std::optional<CHyprColor> m_forcedColor;

    friend class CPillPassElement;
};
