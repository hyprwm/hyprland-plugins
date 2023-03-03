#include "borderDeco.hpp"

#include <src/Compositor.hpp>
#include <src/Window.hpp>

#include "globals.hpp"

CBordersPlusPlus::CBordersPlusPlus(CWindow* pWindow) {
    m_pWindow         = pWindow;
    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();
}

CBordersPlusPlus::~CBordersPlusPlus() {
    damageEntire();
}

SWindowDecorationExtents CBordersPlusPlus::getWindowDecorationExtents() {
    return m_seExtents;
}

void CBordersPlusPlus::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static auto* const PCOLOR1     = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_1")->intValue;
    static auto* const PCOLOR2     = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_2")->intValue;
    static auto* const PBORDERS    = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders")->intValue;
    static auto* const PROUNDING   = &HyprlandAPI::getConfigValue(PHANDLE, "decoration:rounding")->intValue;
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;

    if (*PBORDERS < 1)
        return;

    const auto ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
        0 :
        (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    // draw the border
    wlr_box fullBox = {(int)(m_vLastWindowPos.x - *PBORDERSIZE), (int)(m_vLastWindowPos.y - *PBORDERSIZE), (int)(m_vLastWindowSize.x + 2.0 * *PBORDERSIZE),
                       (int)(m_vLastWindowSize.y + 2.0 * *PBORDERSIZE)};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.x += offset.x;
    fullBox.y += offset.y;

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);

    scaleBox(&fullBox, pMonitor->scale);
    g_pHyprOpenGL->renderBorder(&fullBox, CColor(*PCOLOR1), *PROUNDING * pMonitor->scale + *PBORDERSIZE * 2, a);

    // pass 2

    if (*PBORDERS < 2)
        return;

    fullBox = {(int)(m_vLastWindowPos.x - *PBORDERSIZE * 2), (int)(m_vLastWindowPos.y - *PBORDERSIZE * 2), (int)(m_vLastWindowSize.x + 2.0 * *PBORDERSIZE * 2),
               (int)(m_vLastWindowSize.y + 2.0 * *PBORDERSIZE * 2)};

    fullBox.x -= pMonitor->vecPosition.x;
    fullBox.y -= pMonitor->vecPosition.y;

    m_seExtents = {{m_vLastWindowPos.x - fullBox.x - pMonitor->vecPosition.x + 2, m_vLastWindowPos.y - fullBox.y - pMonitor->vecPosition.y + 2},
                   {fullBox.x + fullBox.width + pMonitor->vecPosition.x - m_vLastWindowPos.x - m_vLastWindowSize.x + 2,
                    fullBox.y + fullBox.height + pMonitor->vecPosition.y - m_vLastWindowPos.y - m_vLastWindowSize.y + 2}};

    fullBox.x += offset.x;
    fullBox.y += offset.y;

    if (fullBox.width < 1 || fullBox.height < 1)
        return; // don't draw invisible shadows

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);

    scaleBox(&fullBox, pMonitor->scale);
    g_pHyprOpenGL->renderBorder(&fullBox, CColor(*PCOLOR2), *PROUNDING * pMonitor->scale + *PBORDERSIZE * 4, a);
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