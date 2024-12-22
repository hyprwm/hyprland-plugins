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
    return false;
}

bool CBarPassElement::needsPrecomputeBlur() {
    return false;
}