#include "BarPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "barDeco.hpp"

CBarPassElement::CBarPassElement(const CBarPassElement::SBarData& data_) : data(data_) {
    ;
}

void CBarPassElement::draw(const CRegion& damage) {
    data.deco->renderPass(g_pHyprOpenGL->m_RenderData.pMonitor.lock(), data.a);
}

bool CBarPassElement::needsLiveBlur() {
    static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();
    static auto* const PENABLEBLUR       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_blur")->getDataStaticPtr();
    static auto* const PENABLEBLURGLOBAL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();

    CHyprColor         color = data.deco->m_bForcedBarColor.value_or(**PCOLOR);
    color.a *= data.a;
    const bool SHOULDBLUR = **PENABLEBLUR && **PENABLEBLURGLOBAL && color.a < 1.F;

    return SHOULDBLUR;
}

std::optional<CBox> CBarPassElement::boundingBox() {
    // Temporary fix: expand the bar bb a bit, otherwise occlusion gets too aggressive.
    return data.deco->assignedBoxGlobal().translate(-g_pHyprOpenGL->m_RenderData.pMonitor->m_position).expand(10);
}

bool CBarPassElement::needsPrecomputeBlur() {
    return false;
}