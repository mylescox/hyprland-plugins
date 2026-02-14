#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include "globals.hpp"

class CLiquidDock {
  public:
    CLiquidDock();
    ~CLiquidDock();

    void         renderPass(PHLMONITOR monitor, float const& a);
    CBox         dockBoxGlobal() const;
    PHLMONITOR   getTargetMonitor() const;
    void         onWindowOpen(PHLWINDOW window);
    void         onWindowClose(PHLWINDOW window);
    void         onWindowFocus(PHLWINDOW window);
    void         damageEntire();

    WP<CLiquidDock> m_self;

  private:
    // Dock geometry (animated)
    PHLANIMVAR<Vector2D> m_vPosition;
    PHLANIMVAR<Vector2D> m_vSize;
    PHLANIMVAR<float>    m_fAlpha;

    // Auto-hide state
    bool                 m_bAutoHideActive = false;
    Time::steady_tp      m_lastHoverTime   = Time::steadyNow();

    // Shader program
    GLuint               m_shaderProgram = 0;

    // Icon management
    void                 rebuildDockItems();
    void                 layoutIcons();
    void                 renderDockSDF(PHLMONITOR monitor, float alpha);
    void                 renderDockIcons(PHLMONITOR monitor, float alpha);

    // Input handling
    void                 onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e);
    void                 onMouseMove(Vector2D coords);
    int                  hitTestIcon(Vector2D coords) const;
    void                 launchApp(const SDockItem& item);

    // Magnification effect
    float                getMagnificationScale(int iconIndex, float cursorX) const;

    // Shader helpers
    void                 initShader();
    void                 destroyShader();

    // Callbacks
    SP<HOOK_CALLBACK_FN> m_pMouseButtonCallback;
    SP<HOOK_CALLBACK_FN> m_pMouseMoveCallback;

    // Cached dock dimensions
    float                m_fDockWidth     = 0;
    float                m_fDockHeight    = 0;
    float                m_fCursorX       = -1.F;
    float                m_fCursorY       = -1.F;
    bool                 m_bItemsDirty    = true;

    friend class CDockPassElement;
};
