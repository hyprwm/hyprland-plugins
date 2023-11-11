#include "borderDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>

#include "globals.hpp"

CBordersPlusPlus::CBordersPlusPlus(CWindow* pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();
}

CBordersPlusPlus::~CBordersPlusPlus() {
    damageEntire();
}

SDecorationPositioningInfo CBordersPlusPlus::getPositioningInfo() {
    static auto* const           PBORDERS = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders")->intValue;

    static std::vector<int64_t*> PSIZES;
    for (size_t i = 0; i < 9; ++i) {
        PSIZES.push_back(&HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1))->intValue);
    }

    SDecorationPositioningInfo info;
    info.policy   = DECORATION_POSITION_ABSOLUTE;
    info.reserved = true;
    info.priority = 1005;
    info.edges    = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    if (m_fLastThickness == 0) {
        double size = 0;

        for (size_t i = 0; i < *PBORDERS; ++i) {
            size += *PSIZES[i];
        }

        info.desiredExtents = {{size, size}, {size, size}};
        m_fLastThickness    = size;
    } else {
        info.desiredExtents = {{m_fLastThickness, m_fLastThickness}, {m_fLastThickness, m_fLastThickness}};
    }

    return info;
}

void CBordersPlusPlus::onPositioningReply(const SDecorationPositioningReply& reply) {
    ; // ignored
}

uint64_t CBordersPlusPlus::getDecorationFlags() {
    return 0;
}

eDecorationLayer CBordersPlusPlus::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

void CBordersPlusPlus::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static std::vector<int64_t*> PCOLORS;
    static std::vector<int64_t*> PSIZES;
    for (size_t i = 0; i < 9; ++i) {
        PCOLORS.push_back(&HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_" + std::to_string(i + 1))->intValue);
        PSIZES.push_back(&HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1))->intValue);
    }
    static auto* const PBORDERS      = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders")->intValue;
    static auto* const PNATURALROUND = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:natural_rounding")->intValue;
    static auto* const PROUNDING     = &HyprlandAPI::getConfigValue(PHANDLE, "decoration:rounding")->intValue;
    static auto* const PBORDERSIZE   = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;

    if (*PBORDERS < 1)
        return;

    auto       rounding      = m_pWindow->rounding() == 0 ? 0 : m_pWindow->rounding() * pMonitor->scale + *PBORDERSIZE;
    const auto ORIGINALROUND = rounding == 0 ? 0 : m_pWindow->rounding() * pMonitor->scale + *PBORDERSIZE;
    CBox       fullBox       = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};

    fullBox.translate(offset - pMonitor->vecPosition);

    double fullThickness = 0;

    fullBox.x -= *PBORDERSIZE;
    fullBox.y -= *PBORDERSIZE;
    fullBox.width += *PBORDERSIZE * 2;
    fullBox.height += *PBORDERSIZE * 2;

    for (size_t i = 0; i < *PBORDERS; ++i) {
        const int PREVBORDERSIZE = i == 0 ? 0 : (*PSIZES[i - 1] == -1 ? *PBORDERSIZE : *PSIZES[i - 1]);
        const int THISBORDERSIZE = *PSIZES[i] == -1 ? *PBORDERSIZE : *PSIZES[i];

        if (i != 0) {
            rounding += rounding == 0 ? 0 : PREVBORDERSIZE;
            fullBox.x -= PREVBORDERSIZE;
            fullBox.y -= PREVBORDERSIZE;
            fullBox.width += PREVBORDERSIZE * 2;
            fullBox.height += PREVBORDERSIZE * 2;
        }

        if (fullBox.width < 1 || fullBox.height < 1)
            break;

        g_pHyprOpenGL->scissor((CBox*)nullptr);
        CBox saveBox = fullBox;
        saveBox.scale(pMonitor->scale).round();

        g_pHyprOpenGL->renderBorder(&fullBox, CColor{(uint64_t)*PCOLORS[i]}, *PNATURALROUND ? ORIGINALROUND : rounding, THISBORDERSIZE, a, *PNATURALROUND ? ORIGINALROUND : -1);

        fullBox = saveBox;

        fullThickness += THISBORDERSIZE;
    }

    m_seExtents = {{fullThickness, fullThickness}, {fullThickness, fullThickness}};

    if (fullThickness != m_fLastThickness) {
        m_fLastThickness = fullThickness;
        g_pDecorationPositioner->repositionDeco(this);
    }
}

eDecorationType CBordersPlusPlus::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CBordersPlusPlus::updateWindow(CWindow* pWindow) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
    m_vLastWindowSize = pWindow->m_vRealSize.vec();

    damageEntire();
}

void CBordersPlusPlus::damageEntire() {
    CBox dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
               (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}