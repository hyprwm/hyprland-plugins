#include "BorderppPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "borderDeco.hpp"

CBorderPPPassElement::CBorderPPPassElement(const CBorderPPPassElement::SBorderPPData& data_) : data(data_) {
    ;
}

void CBorderPPPassElement::draw(const CRegion& damage) {
    data.deco->drawPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), data.a);
}

bool CBorderPPPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPPPassElement::needsPrecomputeBlur() {
    return false;
}