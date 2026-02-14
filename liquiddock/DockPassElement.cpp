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
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:liquiddock:enabled")->getDataStaticPtr();
    if (!**PENABLED)
        return false;
    return **PBLUR && **PBLURGLB;
}

bool CDockPassElement::needsPrecomputeBlur() {
    return false;
}

bool CDockPassElement::undiscardable() {
    return true;
}

std::optional<CBox> CDockPassElement::boundingBox() {
    const auto renderMonitor = g_pHyprOpenGL->m_renderData.pMonitor.lock();
    if (!renderMonitor)
        return std::nullopt;

    const auto targetMonitor = data.dock->getTargetMonitor();
    if (!targetMonitor || targetMonitor != renderMonitor)
        return CBox{};

    return data.dock->dockBoxGlobal().translate(-renderMonitor->m_position).expand(10);
}
