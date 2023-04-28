#include "barDeco.hpp"

#include <src/Compositor.hpp>
#include <src/Window.hpp>

#include "globals.hpp"

constexpr int BUTTONS_PAD      = 5;
constexpr int BUTTONS_SIZE     = 10;
constexpr int BUTTONS_ROUNDING = 5;

CHyprBar::CHyprBar(CWindow* pWindow) {
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

    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;

    if (!VECINRECT(COORDS, 0, 0, m_vLastWindowSize.x, *PHEIGHT)) {

        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            Debug::log(LOG, "[hyprbars] Dragging ended on %x", m_pWindow);
        }

        m_bDraggingThis = false;
        m_bDragPending  = false;
        return;
    }

    if (e->state != WLR_BUTTON_PRESSED) {
        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            m_bDraggingThis = false;

            Debug::log(LOG, "[hyprbars] Dragging ended on %x", m_pWindow);
        }

        return;
    }

    // check if on a button
    static auto* const PBORDERSIZE = &HyprlandAPI::getConfigValue(PHANDLE, "general:border_size")->intValue;
    const auto         BARBUF      = Vector2D{(int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT};
    Vector2D           currentPos  = Vector2D{BARBUF.x - BUTTONS_PAD - BUTTONS_SIZE, BARBUF.y / 2.0 - BUTTONS_SIZE / 2.0}.floor();
    currentPos.y -= BUTTONS_PAD / 2.0;

    currentPos.x -= BUTTONS_PAD / 2.0;

    if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + BUTTONS_SIZE + BUTTONS_PAD, currentPos.y + BUTTONS_SIZE + BUTTONS_PAD)) {
        // hit on close
        g_pCompositor->closeWindow(m_pWindow);
        return;
    }

    currentPos.x -= BUTTONS_PAD + BUTTONS_SIZE;

    if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + BUTTONS_SIZE + BUTTONS_PAD, currentPos.y + BUTTONS_SIZE + BUTTONS_PAD)) {
        // hit on maximize
        g_pKeybindManager->m_mDispatchers["fullscreen"]("1");
        return;
    }

    m_bDragPending = true;
}

void CHyprBar::onMouseMove(Vector2D coords) {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;

    if (m_bDragPending) {
        m_bDragPending = false;
        g_pKeybindManager->m_mDispatchers["mouse"]("1movewindow");
        m_bDraggingThis = true;

        Debug::log(LOG, "[hyprbars] Dragging initiated on %x", m_pWindow);

        return;
    }
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

void CHyprBar::renderBarButtons(const Vector2D& bufferSize) {

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw buttons for close and max

    auto drawButton = [&](Vector2D pos, CColor col) -> void {
        const int        X       = pos.x;
        const int        Y       = pos.y;
        const int        WIDTH   = BUTTONS_SIZE;
        const int        HEIGHT  = BUTTONS_SIZE;
        const int        RADIUS  = BUTTONS_ROUNDING;
        constexpr double DEGREES = M_PI / 180.0;

        cairo_set_source_rgba(CAIRO, col.r, col.g, col.b, col.a);

        cairo_new_sub_path(CAIRO);
        cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + RADIUS, RADIUS, -90 * DEGREES, 0 * DEGREES);
        cairo_arc(CAIRO, X + WIDTH - RADIUS, Y + HEIGHT - RADIUS, RADIUS, 0 * DEGREES, 90 * DEGREES);
        cairo_arc(CAIRO, X + RADIUS, Y + HEIGHT - RADIUS, RADIUS, 90 * DEGREES, 180 * DEGREES);
        cairo_arc(CAIRO, X + RADIUS, Y + RADIUS, RADIUS, 180 * DEGREES, 270 * DEGREES);
        cairo_close_path(CAIRO);

        cairo_fill_preserve(CAIRO);
    };

    Vector2D currentPos = Vector2D{bufferSize.x - BUTTONS_PAD - BUTTONS_SIZE, bufferSize.y / 2.0 - BUTTONS_SIZE / 2.0}.floor();

    drawButton(currentPos, CColor(1.0, 0.0, 0.0, 0.8));

    currentPos.x -= BUTTONS_PAD + BUTTONS_SIZE;

    drawButton(currentPos, CColor(0.9, 0.9, 0.1, 0.8));

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

    CColor             color = *PCOLOR;
    color.a *= a;

    const auto ROUNDING = !m_pWindow->m_sSpecialRenderData.rounding ?
        0 :
        (m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying() == -1 ? *PROUNDING : m_pWindow->m_sAdditionalConfigData.rounding.toUnderlying());

    m_seExtents = {{0, *PHEIGHT + 1}, {}};

    const auto BARBUF = Vector2D{(int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT} * pMonitor->scale;

    wlr_box    titleBarBox = {(int)m_vLastWindowPos.x - *PBORDERSIZE - pMonitor->vecPosition.x, (int)m_vLastWindowPos.y - *PHEIGHT - pMonitor->vecPosition.y,
                           (int)m_vLastWindowSize.x + 2 * *PBORDERSIZE, *PHEIGHT + *PROUNDING * 2 /* to fill the bottom cuz we can't disable rounding there */};

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
        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), *PROUNDING + *PBORDERSIZE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    g_pHyprOpenGL->renderRect(&titleBarBox, color, *PROUNDING);

    // render title
    if (m_szLastTitle != m_pWindow->m_szTitle || m_bWindowSizeChanged) {
        m_szLastTitle = m_pWindow->m_szTitle;
        renderBarTitle(BARBUF);
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

    renderBarButtons(BARBUF);

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
    wlr_box dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
                  (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}

SWindowDecorationExtents CHyprBar::getWindowDecorationReservedArea() {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    return SWindowDecorationExtents{{0, *PHEIGHT}, {}};
}

Vector2D CHyprBar::cursorRelativeToBar() {
    static auto* const PHEIGHT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->intValue;
    return g_pInputManager->getMouseCoordsInternal() - m_pWindow->m_vRealPosition.vec() + Vector2D{0, *PHEIGHT};
}