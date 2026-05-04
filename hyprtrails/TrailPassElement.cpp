#include "TrailPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include "trail.hpp"

CTrailPassElement::CTrailPassElement(const CTrailPassElement::STrailData& data_) : data(data_) {
    ;
}

std::vector<UP<IPassElement>> CTrailPassElement::draw() {
    data.deco->renderPass(g_pHyprRenderer->m_renderData.pMonitor.lock(), data.a);
    return {};
}

bool CTrailPassElement::needsLiveBlur() {
    return false;
}

bool CTrailPassElement::needsPrecomputeBlur() {
    return false;
}
