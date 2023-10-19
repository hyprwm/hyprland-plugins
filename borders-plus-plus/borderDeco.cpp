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

SWindowDecorationExtents CBordersPlusPlus::getWindowDecorationExtents() {
    return m_seExtents;
}

SWindowDecorationExtents CBordersPlusPlus::getWindowDecorationReservedArea() {
    return m_seExtents;
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
    wlr_box    fullBox       = {(int)m_vLastWindowPos.x, (int)m_vLastWindowPos.y, (int)m_vLastWindowSize.x, (int)m_vLastWindowSize.y};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    fullBox.x += offset.x;
    fullBox.y += offset.y;

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

        g_pHyprOpenGL->scissor((wlr_box*)nullptr);
        wlr_box saveBox = fullBox;

        scaleBox(&fullBox, pMonitor->scale);

        g_pHyprOpenGL->renderBorder(&fullBox, CColor{(uint64_t)*PCOLORS[i]}, *PNATURALROUND ? ORIGINALROUND : rounding, THISBORDERSIZE, a, *PNATURALROUND ? ORIGINALROUND : -1);

        fullBox = saveBox;

        fullThickness += THISBORDERSIZE;
    }

    m_seExtents = {{fullThickness, fullThickness}, {fullThickness, fullThickness}};
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
    wlr_box dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
                  (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}