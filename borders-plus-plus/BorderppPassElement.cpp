#include "BorderppPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include "borderDeco.hpp"

CBorderPPPassElement::CBorderPPPassElement(const CBorderPPPassElement::SBorderPPData& data_) : data(data_) {
    ;
}

std::vector<UP<IPassElement>> CBorderPPPassElement::draw() {
    data.deco->drawPass(g_pHyprRenderer->m_renderData.pMonitor.lock(), data.a);
    return {};
}

bool CBorderPPPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPPPassElement::needsPrecomputeBlur() {
    return false;
}
