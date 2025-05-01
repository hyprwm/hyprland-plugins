#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include "overview.hpp"

COverviewPassElement::COverviewPassElement() {
    ;
}

void COverviewPassElement::draw(const CRegion& damage) {
    g_pOverview->fullRender();
}

bool COverviewPassElement::needsLiveBlur() {
    return false;
}

bool COverviewPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> COverviewPassElement::boundingBox() {
    if (!g_pOverview->pMonitor)
        return std::nullopt;

    return CBox{{}, g_pOverview->pMonitor->m_size};
}

CRegion COverviewPassElement::opaqueRegion() {
    if (!g_pOverview->pMonitor)
        return CRegion{};

    return CBox{{}, g_pOverview->pMonitor->m_size};
}
