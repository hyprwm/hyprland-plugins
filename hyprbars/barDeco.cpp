#include "barDeco.hpp"

#include <src/Compositor.hpp>
#include <src/Window.hpp>

#include "globals.hpp"

CHyprBar::CHyprBar(CWindow* pWindow) {
    m_pWindow         = pWindow;
    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();
}

CHyprBar::~CHyprBar() {
    damageEntire();
}

SWindowDecorationExtents CHyprBar::getWindowDecorationExtents() {
    return m_seExtents;
}

void CHyprBar::renderBarTitle(const Vector2D& bufferSize) {
    static auto* const PCOLOR = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_color")->intValue;
    static auto* const PSIZE  = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size")->intValue;
    static auto* const PFONT  = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font")->strValue;

    const CColor       COLOR = *PCOLOR;

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);

    const auto         CAIRO = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title
    cairo_select_font_face(CAIRO, PFONT->c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(CAIRO, *PSIZE);
    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    cairo_text_extents_t cairoExtents;
    cairo_text_extents(CAIRO, m_szLastTitle.c_str(), &cairoExtents);

    cairo_move_to(CAIRO, std::round(bufferSize.x / 2.0 - cairoExtents.width / 2.0), *PSIZE + std::round(bufferSize.y / 2.0 - cairoExtents.height / 2.0));
    cairo_show_text(CAIRO, m_szLastTitle.c_str());

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_tTextTex.allocate();
    glBindTexture(GL_TEXTURE_2D, m_tTextTex.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void CHyprBar::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static auto* const PROUNDING   = &HyprlandAPI::getConfigValue(PHANDLE, "decoration:rounding")->intValue;
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;
    static auto* const PCOLOR      = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->intValue;
    static auto* const PHEIGHT     = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;

    CColor             color = *PCOLOR;
    color.a *= a;

    const auto ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
        0 :
        (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    // title bar's extents are 0 because we cut into the window
    m_seExtents = {{}, {}};

    const auto BARBUF = Vector2D{(int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT} * pMonitor->scale;

    wlr_box    titleBarBox = {(int)m_vLastWindowPos.x - *PBORDERSIZE, (int)m_vLastWindowPos.y, (int)m_vLastWindowSize.x + 2 * *PBORDERSIZE,
                           *PHEIGHT + *PROUNDING * 2 /* to fill the bottom cuz we can't disable rounding there */};

    titleBarBox.x += offset.x;
    titleBarBox.y += offset.y;

    scaleBox(&titleBarBox, pMonitor->scale);

    g_pHyprOpenGL->scissor(&titleBarBox);

    if (*PROUNDING) {
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glEnable(GL_STENCIL_TEST);

        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        wlr_box windowBox = {(int)m_vLastWindowPos.x - *PBORDERSIZE + offset.x, (int)m_vLastWindowPos.y - *PBORDERSIZE + offset.y, (int)m_vLastWindowSize.x + 2 * *PBORDERSIZE,
                             (int)m_vLastWindowSize.y + 2 * *PBORDERSIZE};
        scaleBox(&windowBox, pMonitor->scale);
        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), *PROUNDING + *PBORDERSIZE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_EQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    g_pHyprOpenGL->renderRect(&titleBarBox, color, *PROUNDING);

    // render title
    if (m_szLastTitle != m_pWindow->m_szTitle) {
        m_szLastTitle = m_pWindow->m_szTitle;
        renderBarTitle(BARBUF);
    }

    wlr_box textBox = {titleBarBox.x, titleBarBox.y, (int)BARBUF.x, (int)BARBUF.y};
    g_pHyprOpenGL->renderTexture(m_tTextTex, &textBox, a);

    // TODO: buttons and shit

    if (*PROUNDING) {
        // cleanup stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    }

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);
}

eDecorationType CHyprBar::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CHyprBar::updateWindow(CWindow* pWindow) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
    m_vLastWindowSize = pWindow->m_vRealSize.vec();

    damageEntire();
}

void CHyprBar::damageEntire() {
    wlr_box dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
                  (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}

SWindowDecorationExtents CHyprBar::getWindowDecorationReservedArea() {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    return SWindowDecorationExtents{{0, *PHEIGHT}, {}};
}