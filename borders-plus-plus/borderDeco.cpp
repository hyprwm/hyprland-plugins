#include "borderDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>

#include "globals.hpp"

CBordersPlusPlus::CBordersPlusPlus(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_vLastWindowPos  = pWindow->m_vRealPosition.value();
    m_vLastWindowSize = pWindow->m_vRealSize.value();
}

CBordersPlusPlus::~CBordersPlusPlus() {
    damageEntire();
}

SDecorationPositioningInfo CBordersPlusPlus::getPositioningInfo() {
    static auto* const                        PBORDERS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders")->getDataStaticPtr();

    static std::vector<Hyprlang::INT* const*> PSIZES;
    for (size_t i = 0; i < 9; ++i) {
        PSIZES.push_back((Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1))->getDataStaticPtr());
    }

    SDecorationPositioningInfo info;
    info.policy   = DECORATION_POSITION_ABSOLUTE;
    info.reserved = true;
    info.priority = 9990;
    info.edges    = DECORATION_EDGE_BOTTOM | DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT | DECORATION_EDGE_TOP;

    if (m_fLastThickness == 0) {
        double size = 0;

        for (size_t i = 0; i < **PBORDERS; ++i) {
            size += **PSIZES[i];
        }

        info.desiredExtents = {{size, size}, {size, size}};
        m_fLastThickness    = size;
    } else
        info.desiredExtents = {{m_fLastThickness, m_fLastThickness}, {m_fLastThickness, m_fLastThickness}};

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

std::string CBordersPlusPlus::getDisplayName() {
    return "Borders++";
}

void CBordersPlusPlus::draw(CMonitor* pMonitor, float a) {
    if (!validMapped(m_pWindow))
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_sWindowData.decorate.valueOrDefault())
        return;

    static std::vector<Hyprlang::INT* const*> PCOLORS;
    static std::vector<Hyprlang::INT* const*> PSIZES;
    for (size_t i = 0; i < 9; ++i) {
        PCOLORS.push_back((Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_" + std::to_string(i + 1))->getDataStaticPtr());
        PSIZES.push_back((Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1))->getDataStaticPtr());
    }
    static auto* const PBORDERS      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders")->getDataStaticPtr();
    static auto* const PNATURALROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:borders-plus-plus:natural_rounding")->getDataStaticPtr();
    static auto* const PROUNDING     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:rounding")->getDataStaticPtr();
    static auto* const PBORDERSIZE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->getDataStaticPtr();

    if (**PBORDERS < 1)
        return;

    const auto PWORKSPACE      = PWINDOW->m_pWorkspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();

    auto       rounding      = PWINDOW->rounding() == 0 ? 0 : PWINDOW->rounding() * pMonitor->scale + **PBORDERSIZE;
    const auto ORIGINALROUND = rounding == 0 ? 0 : PWINDOW->rounding() * pMonitor->scale + **PBORDERSIZE;
    CBox       fullBox       = {m_vLastWindowPos.x, m_vLastWindowPos.y, m_vLastWindowSize.x, m_vLastWindowSize.y};

    fullBox.translate(PWINDOW->m_vFloatingOffset - pMonitor->vecPosition + WORKSPACEOFFSET).scale(pMonitor->scale).expand(**PBORDERSIZE * pMonitor->scale);

    double fullThickness = 0;

    for (size_t i = 0; i < **PBORDERS; ++i) {
        const int PREVBORDERSIZESCALED = i == 0 ? 0 : (**PSIZES[i - 1] == -1 ? **PBORDERSIZE : **(PSIZES[i - 1])) * pMonitor->scale;
        const int THISBORDERSIZE       = **(PSIZES[i]) == -1 ? **PBORDERSIZE : (**PSIZES[i]);

        if (i != 0) {
            rounding += rounding == 0 ? 0 : PREVBORDERSIZESCALED / pMonitor->scale;
            fullBox.x -= PREVBORDERSIZESCALED;
            fullBox.y -= PREVBORDERSIZESCALED;
            fullBox.width += PREVBORDERSIZESCALED * 2;
            fullBox.height += PREVBORDERSIZESCALED * 2;
        }

        if (fullBox.width < 1 || fullBox.height < 1)
            break;

        g_pHyprOpenGL->scissor((CBox*)nullptr);

        g_pHyprOpenGL->renderBorder(&fullBox, CColor{(uint64_t) * *PCOLORS[i]}, **PNATURALROUND ? ORIGINALROUND : rounding, THISBORDERSIZE, a,
                                    **PNATURALROUND ? ORIGINALROUND : -1);

        fullThickness += THISBORDERSIZE;
    }

    m_seExtents = {{fullThickness, fullThickness}, {fullThickness, fullThickness}};

    m_bLastRelativeBox = CBox{0, 0, m_vLastWindowSize.x, m_vLastWindowSize.y}.expand(**PBORDERSIZE).addExtents(m_seExtents);

    if (fullThickness != m_fLastThickness) {
        m_fLastThickness = fullThickness;
        g_pDecorationPositioner->repositionDeco(this);
    }
}

eDecorationType CBordersPlusPlus::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CBordersPlusPlus::updateWindow(PHLWINDOW pWindow) {
    m_vLastWindowPos  = pWindow->m_vRealPosition.value();
    m_vLastWindowSize = pWindow->m_vRealSize.value();

    damageEntire();
}

void CBordersPlusPlus::damageEntire() {
    CBox dm = m_bLastRelativeBox.copy().translate(m_vLastWindowPos).expand(2);
    g_pHyprRenderer->damageBox(&dm);
}
