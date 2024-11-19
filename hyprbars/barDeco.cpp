#include "barDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <pango/pangocairo.h>

#include "globals.hpp"

CHyprBar::CHyprBar(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;

    const auto PMONITOR       = pWindow->m_pMonitor.lock();
    PMONITOR->scheduledRecalc = true;

    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseDown(info, std::any_cast<IPointer::SButtonEvent>(param)); });

    m_pMouseMoveCallback =
        HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });

    m_pTextTex    = makeShared<CTexture>();
    m_pButtonsTex = makeShared<CTexture>();
}

CHyprBar::~CHyprBar() {
    damageEntire();
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
    std::erase(g_pGlobalState->bars, this);
}

SDecorationPositioningInfo CHyprBar::getPositioningInfo() {
    static auto* const         PHEIGHT     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const         PPRECEDENCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();

    SDecorationPositioningInfo info;
    info.policy         = m_bHidden ? DECORATION_POSITION_ABSOLUTE : DECORATION_POSITION_STICKY;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = **PPRECEDENCE ? 10005 : 5000;
    info.reserved       = true;
    info.desiredExtents = {{0, m_bHidden ? 0 : **PHEIGHT}, {0, 0}};
    return info;
}

void CHyprBar::onPositioningReply(const SDecorationPositioningReply& reply) {
    if (reply.assignedGeometry.size() != m_bAssignedBox.size())
        m_bWindowSizeChanged = true;

    m_bAssignedBox = reply.assignedGeometry;
}

std::string CHyprBar::getDisplayName() {
    return "Hyprbar";
}

void CHyprBar::onMouseDown(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (m_pWindow.lock() != g_pCompositor->m_pLastWindow.lock())
        return;

    const auto         PWINDOW = m_pWindow.lock();

    const auto         COORDS = cursorRelativeToBar();

    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";

    if (!VECINRECT(COORDS, 0, 0, assignedBoxGlobal().w, **PHEIGHT - 1)) {

        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            Debug::log(LOG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)PWINDOW.get());
        }

        m_bDraggingThis = false;
        m_bDragPending  = false;
        return;
    }

    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED) {

        if (m_bCancelledDown)
            info.cancelled = true;

        m_bCancelledDown = false;

        if (m_bDraggingThis) {
            g_pKeybindManager->m_mDispatchers["mouse"]("0movewindow");
            m_bDraggingThis = false;

            Debug::log(LOG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)PWINDOW.get());
        }

        m_bDragPending = false;

        return;
    }

    if (PWINDOW->m_bIsFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    info.cancelled   = true;
    m_bCancelledDown = true;

    // check if on a button

    float offset = **PBARPADDING;

    for (auto& b : g_pGlobalState->buttons) {
        const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, **PHEIGHT};
        Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - **PBARBUTTONPADDING - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

        if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + **PBARBUTTONPADDING, currentPos.y + b.size)) {
            // hit on close
            g_pKeybindManager->m_mDispatchers["exec"](b.cmd);
            return;
        }

        offset += **PBARBUTTONPADDING + b.size;
    }

    m_bDragPending = true;
}

void CHyprBar::onMouseMove(Vector2D coords) {
    if (m_bDragPending) {
        m_bDragPending = false;
        g_pKeybindManager->m_mDispatchers["mouse"]("1movewindow");
        m_bDraggingThis = true;

        Debug::log(LOG, "[hyprbars] Dragging initiated on {:x}", (uintptr_t)m_pWindow.lock().get());

        return;
    }
}

void CHyprBar::renderText(SP<CTexture> out, const std::string& text, const CColor& color, const Vector2D& bufferSize, const float scale, const int fontSize) {
    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, text.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string("sans");
    pango_font_description_set_size(fontDesc, fontSize * scale * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    const int maxWidth = bufferSize.x;

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);

    cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);

    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);
    const double xOffset = (bufferSize.x / 2.0 - layoutWidth / PANGO_SCALE / 2.0);
    const double yOffset = (bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0);

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    out->allocate();
    glBindTexture(GL_TEXTURE_2D, out->m_iTexID);
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

void CHyprBar::renderBarTitle(const Vector2D& bufferSize, const float scale) {
    static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:col.text")->getDataStaticPtr();
    static auto* const PSIZE             = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size")->getDataStaticPtr();
    static auto* const PFONT             = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font")->getDataStaticPtr();
    static auto* const PALIGN            = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_text_align")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";

    const auto         PWINDOW = m_pWindow.lock();

    const auto         BORDERSIZE = PWINDOW->getRealBorderSize();

    float              buttonSizes = **PBARBUTTONPADDING;
    for (auto& b : g_pGlobalState->buttons) {
        buttonSizes += b.size + **PBARBUTTONPADDING;
    }

    const auto   scaledSize        = **PSIZE * scale;
    const auto   scaledBorderSize  = BORDERSIZE * scale;
    const auto   scaledButtonsSize = buttonSizes * scale;
    const auto   scaledButtonsPad  = **PBARBUTTONPADDING * scale;
    const auto   scaledBarPadding  = **PBARPADDING * scale;

    const CColor COLOR = m_bForcedTitleColor.value_or(**PCOLOR);

    const auto   CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto   CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, m_szLastTitle.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string(*PFONT);
    pango_font_description_set_size(fontDesc, scaledSize * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    const int paddingTotal = scaledBarPadding * 2 + scaledButtonsSize + (std::string{*PALIGN} != "left" ? scaledButtonsSize : 0);
    const int maxWidth     = std::clamp(static_cast<int>(bufferSize.x - paddingTotal), 0, INT_MAX);

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);
    const int xOffset = std::string{*PALIGN} == "left" ? std::round(scaledBarPadding + (BUTTONSRIGHT ? 0 : scaledButtonsSize)) :
                                                         std::round(((bufferSize.x - scaledBorderSize) / 2.0 - layoutWidth / PANGO_SCALE / 2.0));
    const int yOffset = std::round((bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0));

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_pTextTex->allocate();
    glBindTexture(GL_TEXTURE_2D, m_pTextTex->m_iTexID);
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
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();

    const auto         scaledButtonsPad = **PBARBUTTONPADDING * scale;
    const auto         scaledBarPadding = **PBARPADDING * scale;

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    static auto* const PALIGNBUTTONS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw buttons
    int  offset = scaledBarPadding;

    auto drawButton = [&](SHyprButton& button) -> void {
        const auto scaledButtonSize = button.size * scale;

        Vector2D   currentPos =
            Vector2D{BUTTONSRIGHT ? bufferSize.x - offset - scaledButtonSize / 2.0 : offset + scaledButtonSize / 2.0, (bufferSize.y - scaledButtonSize) / 2.0}.floor();

        const int X      = currentPos.x;
        const int Y      = currentPos.y;
        const int RADIUS = static_cast<int>(std::ceil(scaledButtonSize / 2.0));

        cairo_set_source_rgba(CAIRO, button.col.r, button.col.g, button.col.b, button.col.a);
        cairo_arc(CAIRO, X, Y + RADIUS, RADIUS, 0, 2 * M_PI);
        cairo_fill(CAIRO);

        offset += scaledButtonsPad + scaledButtonSize;
    };

    for (auto& b : g_pGlobalState->buttons) {
        drawButton(b);
    }

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_pButtonsTex->allocate();
    glBindTexture(GL_TEXTURE_2D, m_pButtonsTex->m_iTexID);
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

void CHyprBar::renderBarButtonsText(CBox* barBox, const float scale, const float a) {
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";

    const auto         scaledButtonsPad = **PBARBUTTONPADDING * scale;
    const auto         scaledBarPad     = **PBARPADDING * scale;
    int                offset           = scaledBarPad;
    //

    auto drawButton = [&](SHyprButton& button) -> void {
        const auto scaledButtonSize = button.size * scale;

        if (button.iconTex->m_iTexID == 0 /* icon is not rendered */ && !button.icon.empty()) {
            // render icon
            const Vector2D BUFSIZE = {scaledButtonSize, scaledButtonSize};

            const bool     LIGHT = button.col.r + button.col.g + button.col.b < 1;

            renderText(button.iconTex, button.icon, LIGHT ? CColor(0xFFFFFFFF) : CColor(0xFF000000), BUFSIZE, scale, button.size * 0.62);
        }

        if (button.iconTex->m_iTexID == 0)
            return;

        CBox pos = {barBox->x + (BUTTONSRIGHT ? barBox->width - offset - scaledButtonSize : offset), barBox->y + (barBox->height - scaledButtonSize) / 2.0, scaledButtonSize,
                    scaledButtonSize};

        g_pHyprOpenGL->renderTexture(button.iconTex, &pos, a);

        offset += scaledButtonsPad + scaledButtonSize;
    };

    for (auto& b : g_pGlobalState->buttons) {
        drawButton(b);
    }
}

void CHyprBar::draw(PHLMONITOR pMonitor, const float &a) {
    if (m_bHidden || !validMapped(m_pWindow))
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_sWindowData.decorate.valueOrDefault())
        return;

    static auto* const PCOLOR        = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();
    static auto* const PHEIGHT       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PPRECEDENCE   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PENABLETITLE  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";

    if (**PHEIGHT < 1) {
        m_iLastHeight = **PHEIGHT;
        return;
    }

    const auto PWORKSPACE      = PWINDOW->m_pWorkspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();

    const auto ROUNDING = PWINDOW->rounding() + (*PPRECEDENCE ? 0 : PWINDOW->getRealBorderSize());

    const auto scaledRounding = ROUNDING > 0 ? ROUNDING * pMonitor->scale - 2 /* idk why but otherwise it looks bad due to the gaps */ : 0;

    CColor     color = m_bForcedBarColor.value_or(**PCOLOR);
    color.a *= a;

    m_seExtents = {{0, **PHEIGHT}, {}};

    const auto DECOBOX = assignedBoxGlobal();

    const auto BARBUF = DECOBOX.size() * pMonitor->scale;

    CBox       titleBarBox = {DECOBOX.x - pMonitor->vecPosition.x, DECOBOX.y - pMonitor->vecPosition.y, DECOBOX.w,
                              DECOBOX.h + ROUNDING * 3 /* to fill the bottom cuz we can't disable rounding there */};

    titleBarBox.translate(PWINDOW->m_vFloatingOffset).scale(pMonitor->scale).round();

    if (titleBarBox.w < 1 || titleBarBox.h < 1)
        return;

    g_pHyprOpenGL->scissor(&titleBarBox);

    if (ROUNDING) {
        // the +1 is a shit garbage temp fix until renderRect supports an alpha matte
        CBox windowBox = {PWINDOW->m_vRealPosition.value().x + PWINDOW->m_vFloatingOffset.x - pMonitor->vecPosition.x + 1,
                          PWINDOW->m_vRealPosition.value().y + PWINDOW->m_vFloatingOffset.y - pMonitor->vecPosition.y + 1, PWINDOW->m_vRealSize.value().x - 2,
                          PWINDOW->m_vRealSize.value().y - 2};

        if (windowBox.w < 1 || windowBox.h < 1)
            return;

        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glEnable(GL_STENCIL_TEST);

        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        windowBox.translate(WORKSPACEOFFSET).scale(pMonitor->scale).round();
        g_pHyprOpenGL->renderRect(&windowBox, CColor(0, 0, 0, 0), scaledRounding);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    g_pHyprOpenGL->renderRect(&titleBarBox, color, scaledRounding);

    // render title
    if (**PENABLETITLE && (m_szLastTitle != PWINDOW->m_szTitle || m_bWindowSizeChanged || m_pTextTex->m_iTexID == 0 || m_bTitleColorChanged)) {
        m_szLastTitle = PWINDOW->m_szTitle;
        renderBarTitle(BARBUF, pMonitor->scale);
    }

    if (ROUNDING) {
        // cleanup stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    }

    CBox textBox = {titleBarBox.x, titleBarBox.y, (int)BARBUF.x, (int)BARBUF.y};
    if (**PENABLETITLE)
        g_pHyprOpenGL->renderTexture(m_pTextTex, &textBox, a);

    if (m_bButtonsDirty || m_bWindowSizeChanged) {
        renderBarButtons(BARBUF, pMonitor->scale);
        m_bButtonsDirty = false;
    }

    g_pHyprOpenGL->renderTexture(m_pButtonsTex, &textBox, a);

    g_pHyprOpenGL->scissor((CBox*)nullptr);

    renderBarButtonsText(&textBox, pMonitor->scale, a);

    m_bWindowSizeChanged = false;
    m_bTitleColorChanged = false;

    // dynamic updates change the extents
    if (m_iLastHeight != **PHEIGHT) {
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(PWINDOW);
        m_iLastHeight = **PHEIGHT;
    }
}

eDecorationType CHyprBar::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CHyprBar::updateWindow(PHLWINDOW pWindow) {
    damageEntire();
}

void CHyprBar::damageEntire() {
    ; // ignored
}

Vector2D CHyprBar::cursorRelativeToBar() {
    return g_pInputManager->getMouseCoordsInternal() - assignedBoxGlobal().pos();
}

eDecorationLayer CHyprBar::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t CHyprBar::getDecorationFlags() {
    static auto* const PPART = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_part_of_window")->getDataStaticPtr();
    return DECORATION_ALLOWS_MOUSE_INPUT | (**PPART ? DECORATION_PART_OF_MAIN_WINDOW : 0);
}

CBox CHyprBar::assignedBoxGlobal() {
    const auto PWINDOW = m_pWindow.lock();

    CBox       box = m_bAssignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, PWINDOW));

    const auto PWORKSPACE      = PWINDOW->m_pWorkspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();

    return box.translate(WORKSPACEOFFSET);
}

PHLWINDOW CHyprBar::getOwner() {
    return m_pWindow.lock();
}

void CHyprBar::updateRules() {
    const auto PWINDOW                  = m_pWindow.lock();
    auto       rules                    = PWINDOW->m_vMatchedRules;
    auto       prev_m_bHidden           = m_bHidden;
    auto       prev_m_bForcedTitleColor = m_bForcedTitleColor;

    m_bForcedBarColor   = std::nullopt;
    m_bForcedTitleColor = std::nullopt;
    m_bHidden           = false;

    for (auto& r : rules) {
        applyRule(r);
    }

    if (prev_m_bHidden != m_bHidden)
        g_pDecorationPositioner->repositionDeco(this);
    if (prev_m_bForcedTitleColor != m_bForcedTitleColor)
        m_bTitleColorChanged = true;
}

void CHyprBar::applyRule(const SWindowRule& r) {
    auto arg = r.szRule.substr(r.szRule.find_first_of(' ') + 1);

    if (r.szRule == "plugin:hyprbars:nobar")
        m_bHidden = true;
    else if (r.szRule.starts_with("plugin:hyprbars:bar_color"))
        m_bForcedBarColor = CColor(configStringToInt(arg).value_or(0));
    else if (r.szRule.starts_with("plugin:hyprbars:title_color"))
        m_bForcedTitleColor = CColor(configStringToInt(arg).value_or(0));
}
