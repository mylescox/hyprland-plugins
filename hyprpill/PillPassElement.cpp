#include "PillPassElement.hpp"

#include <hyprland/src/render/OpenGL.hpp>

#include "globals.hpp"
#include "pillDeco.hpp"

CPillPassElement::CPillPassElement(const CPillPassElement::SPillData& data_) : data(data_) {
    ;
}

void CPillPassElement::draw(const CRegion& damage) {
    data.deco->renderPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), data.a);
}

bool CPillPassElement::needsLiveBlur() {
    static auto* const PBLUR      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprpill:pill_blur")->getDataStaticPtr();
    static auto* const PBLURGLB   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();
    return **PBLUR && **PBLURGLB;
}

bool CPillPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> CPillPassElement::boundingBox() {
    return data.deco->visibleBoxGlobal().translate(-g_pHyprOpenGL->m_renderData.pMonitor->m_position).expand(10);
}
