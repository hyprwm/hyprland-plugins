#include "barDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <pango/pangocairo.h>

#include "globals.hpp"

constexpr int BAR_PADDING = 10;
constexpr int BUTTONS_PAD = 5;

CHyprBar::CHyprBar(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow         = pWindow;
    m_vLastWindowPos  = pWindow->m_vRealPosition.vec();
    m_vLastWindowSize = pWindow->m_vRealSize.vec();

    const auto PMONITOR       = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    PMONITOR->scheduledRecalc = true;

    m_pMouseButtonCallback =
        HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseButton", [&](void* self, std::any param) { onMouseDown(std::any_cast<wlr_pointer_button_event*>(param)); });

    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseMove", [&](void* self, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });
}

CHyprBar::~CHyprBar() {
    damageEntire();
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
}

bool CHyprBar::allowsInput() {
    return true;
}

SWindowDecorationExtents CHyprBar::getWindowDecorationExtents() {
    return m_seExtents;
}

void CHyprBar::onMouseDown(wlr_pointer_button_event* e) {
    if (m_pWindow != g_pCompositor->m_pLastWindow)
        return;

    const auto         COORDS = cursorRelativeToBar();

    static auto* const PHEIGHT     = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;

    if (!VECINRECT(COORDS, 0, 0, m_vLastWindowSize.x + *PBORDERSIZE * 2, *PHEIGHT + *PBORDERSIZE)) {

        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            Debug::log(LOG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)m_pWindow);
        }

        m_bDraggingThis = false;
        m_bDragPending  = false;
        return;
    }

    if (e->state != WLR_BUTTON_PRESSED) {
        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            m_bDraggingThis = false;

            Debug::log(LOG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)m_pWindow);
        }

        return;
    }

    // check if on a button
    static auto* const PBUTTONSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:buttons:button_size")->intValue;

    const auto         BARBUF     = Vector2D{(int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT + *PBORDERSIZE};
    Vector2D           currentPos = Vector2D{BARBUF.x - 2 * BUTTONS_PAD - *PBUTTONSIZE, (BARBUF.y - *PBUTTONSIZE) / 2.0}.floor();

    currentPos.x -= BUTTONS_PAD / 2.0;
    if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + *PBUTTONSIZE + BUTTONS_PAD, currentPos.y + *PBUTTONSIZE)) {
        // hit on close
        g_pCompositor->closeWindow(m_pWindow);
        return;
    }

    currentPos.x -= BUTTONS_PAD + *PBUTTONSIZE;
    if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + *PBUTTONSIZE + BUTTONS_PAD, currentPos.y + *PBUTTONSIZE)) {
        // hit on maximize
        g_pKeybindManager->m_mDispatchers["fullscreen"]("1");
        return;
    }

    m_bDragPending = true;
}

void CHyprBar::onMouseMove(Vector2D coords) {
    if (m_bDragPending) {
        m_bDragPending = false;
        g_pKeybindManager->m_mDispatchers["mouse"]("1movewindow");
        m_bDraggingThis = true;

        Debug::log(LOG, "[hyprbars] Dragging initiated on {:x}", (uintptr_t)m_pWindow);

        return;
    }
}

void CHyprBar::renderBarTitle(const Vector2D& bufferSize, const float scale) {
    static auto* const PCOLOR      = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:col.text")->intValue;
    static auto* const PSIZE       = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size")->intValue;
    static auto* const PFONT       = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font")->strValue;
    static auto* const PBUTTONSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:buttons:button_size")->intValue;
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;

    const auto         scaledSize       = *PSIZE * scale;
    const auto         scaledButtonSize = *PBUTTONSIZE * scale;
    const auto         scaledBorderSize = *PBORDERSIZE * scale;
    const auto         scaledButtonsPad = BUTTONS_PAD * scale;
    const auto         scaledBarPadding = BAR_PADDING * scale;

    const CColor       COLOR = *PCOLOR;

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, m_szLastTitle.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string(PFONT->c_str());
    pango_font_description_set_size(fontDesc, scaledSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    const int leftPadding  = scaledBorderSize + scaledBarPadding;
    const int rightPadding = (scaledButtonSize * 2) + (scaledButtonsPad * 3) + scaledBorderSize + scaledBarPadding;
    const int maxWidth     = bufferSize.x - leftPadding - rightPadding;

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);
    const int xOffset = std::round(((bufferSize.x - scaledBorderSize) / 2.0 - layoutWidth / PANGO_SCALE / 2.0));
    const int yOffset = std::round((bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0));

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

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

void CHyprBar::renderBarButtons(const Vector2D& bufferSize, const float scale) {
    static auto* const PCLOSECOLOR = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:buttons:col.close")->intValue;
    static auto* const PMAXCOLOR   = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:buttons:col.maximize")->intValue;
    static auto* const PBUTTONSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:buttons:button_size")->intValue;

    const auto         scaledButtonSize = *PBUTTONSIZE * scale;
    const auto         scaledButtonsPad = BUTTONS_PAD * scale;

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw buttons for close and max

    auto drawButton = [&](Vector2D pos, CColor col) -> void {
        const int X      = pos.x;
        const int Y      = pos.y;
        const int RADIUS = static_cast<int>(std::ceil(scaledButtonSize / 2.0));

        cairo_set_source_rgba(CAIRO, col.r, col.g, col.b, col.a);
        cairo_arc(CAIRO, X, Y + RADIUS, RADIUS, 0, 2 * M_PI);
        cairo_fill(CAIRO);
    };

    Vector2D currentPos = Vector2D{bufferSize.x - scaledButtonsPad - scaledButtonSize, (bufferSize.y - scaledButtonSize) / 2.0}.floor();

    drawButton(currentPos, CColor(*PCLOSECOLOR));

    currentPos.x -= scaledButtonsPad + scaledButtonSize;

    drawButton(currentPos, CColor(*PMAXCOLOR));

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_tButtonsTex.allocate();
    glBindTexture(GL_TEXTURE_2D, m_tButtonsTex.m_iTexID);
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

    const auto         scaledRounding   = *PROUNDING * pMonitor->scale;
    const auto         scaledBorderSize = *PBORDERSIZE * pMonitor->scale;

    CColor             color = *PCOLOR;
    color.a *= a;

    const auto ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
        0 :
        (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    m_seExtents = {{0, *PHEIGHT + 1}, {}};

    const auto BARBUF = Vector2D{(int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT} * pMonitor->scale;

    wlr_box    titleBarBox = {(int)m_vLastWindowPos.x - *PBORDERSIZE - pMonitor->vecPosition.x, (int)m_vLastWindowPos.y - *PBORDERSIZE - *PHEIGHT - pMonitor->vecPosition.y,
                              (int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT + *PROUNDING * 3 /* to fill the bottom cuz we can't disable rounding there */};

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
        wlr_box windowBox = {(int)m_vLastWindowPos.x + offset.x - pMonitor->vecPosition.x, (int)m_vLastWindowPos.y + offset.y - pMonitor->vecPosition.y, (int)m_vLastWindowSize.x,
                             (int)m_vLastWindowSize.y};
        scaleBox(&windowBox, pMonitor->scale);
        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), scaledRounding + scaledBorderSize);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    g_pHyprOpenGL->renderRect(&titleBarBox, color, scaledRounding);

    // render title
    if (m_szLastTitle != m_pWindow->m_szTitle || m_bWindowSizeChanged || m_tTextTex.m_iTexID == 0) {
        m_szLastTitle = m_pWindow->m_szTitle;
        renderBarTitle(BARBUF, pMonitor->scale);
    }

    if (*PROUNDING) {
        // cleanup stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    }

    wlr_box textBox = {titleBarBox.x, titleBarBox.y, (int)BARBUF.x, (int)BARBUF.y};
    g_pHyprOpenGL->renderTexture(m_tTextTex, &textBox, a);

    renderBarButtons(BARBUF, pMonitor->scale);

    g_pHyprOpenGL->renderTexture(m_tButtonsTex, &textBox, a);

    g_pHyprOpenGL->scissor((wlr_box*)nullptr);

    m_bWindowSizeChanged = false;
}

eDecorationType CHyprBar::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CHyprBar::updateWindow(CWindow* pWindow) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    if (m_vLastWindowSize != pWindow->m_vRealSize.vec())
        m_bWindowSizeChanged = true;

    m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
    m_vLastWindowSize = pWindow->m_vRealSize.vec();

    damageEntire();
}

void CHyprBar::damageEntire() {
    wlr_box dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x - 2), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y - 2),
                  (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x + 4), (int)m_seExtents.topLeft.y + 4};
    g_pHyprRenderer->damageBox(&dm);
}

SWindowDecorationExtents CHyprBar::getWindowDecorationReservedArea() {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    return SWindowDecorationExtents{{0, *PHEIGHT}, {}};
}

Vector2D CHyprBar::cursorRelativeToBar() {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    static auto* const PBORDER = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;
    return g_pInputManager->getMouseCoordsInternal() - m_pWindow->m_vRealPosition.vec() + Vector2D{*PBORDER, *PHEIGHT + *PBORDER};
}
