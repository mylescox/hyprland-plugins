#include "DockPassElement.hpp"

#include <hyprland/src/render/OpenGL.hpp>

#include "globals.hpp"
#include "dockSurface.hpp"

CDockPassElement::CDockPassElement(const CDockPassElement::SDockData& data_) : data(data_) {
    ;
}

void CDockPassElement::draw(const CRegion& damage) {
    data.dock->renderPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), data.a);
}

bool CDockPassElement::needsLiveBlur() {
    static auto* const PBLUR    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:dock_blur")->getDataStaticPtr();
    static auto* const PBLURGLB = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();
    return **PBLUR && **PBLURGLB;
}

bool CDockPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> CDockPassElement::boundingBox() {
    return data.dock->dockBoxGlobal().translate(-g_pHyprOpenGL->m_renderData.pMonitor->m_position).expand(10);
}
