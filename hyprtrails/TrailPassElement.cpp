#include "TrailPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "trail.hpp"

CTrailPassElement::CTrailPassElement(const CTrailPassElement::STrailData& data_) : data(data_) {
    ;
}

void CTrailPassElement::draw(const CRegion& damage) {
    data.deco->renderPass(g_pHyprOpenGL->m_renderData.pMonitor.lock(), data.a);
}

bool CTrailPassElement::needsLiveBlur() {
    return false;
}

bool CTrailPassElement::needsPrecomputeBlur() {
    return false;
}