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
    const auto targetMonitor = data.dock->getTargetMonitor();
    const auto renderMonitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();

    // Only provide a bounding box when rendering the dock's target monitor
    if (!targetMonitor || !renderMonitor || targetMonitor != renderMonitor)
        return std::nullopt;

    return data.dock->dockBoxGlobal().translate(-renderMonitor->m_position).expand(10);
}
