#include "barDeco.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <pango/pangocairo.h>

#include "globals.hpp"
#include "BarPassElement.hpp"

CHyprBar::CHyprBar(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;

    static auto* const PCOLOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();

    const auto         PMONITOR = pWindow->m_monitor.lock();
    PMONITOR->m_scheduledRecalc = true;

    //button events
    m_pMouseButtonCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any param) { onMouseButton(info, std::any_cast<IPointer::SButtonEvent>(param)); });
    m_pTouchDownCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchDown", [&](void* self, SCallbackInfo& info, std::any param) { onTouchDown(info, std::any_cast<ITouch::SDownEvent>(param)); });
    m_pTouchUpCallback = HyprlandAPI::registerCallbackDynamic( //
        PHANDLE, "touchUp", [&](void* self, SCallbackInfo& info, std::any param) { onTouchUp(info, std::any_cast<ITouch::SUpEvent>(param)); });

    //move events
    m_pTouchMoveCallback = HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "touchMove", [&](void* self, SCallbackInfo& info, std::any param) { onTouchMove(info, std::any_cast<ITouch::SMotionEvent>(param)); });
    m_pMouseMoveCallback = HyprlandAPI::registerCallbackDynamic( //
        PHANDLE, "mouseMove", [&](void* self, SCallbackInfo& info, std::any param) { onMouseMove(std::any_cast<Vector2D>(param)); });

    m_pTextTex    = makeShared<CTexture>();
    m_pButtonsTex = makeShared<CTexture>();

    g_pAnimationManager->createAnimation(CHyprColor{**PCOLOR}, m_cRealBarColor, g_pConfigManager->getAnimationPropertyConfig("border"), pWindow, AVARDAMAGE_NONE);
    m_cRealBarColor->setUpdateCallback([&](auto) { damageEntire(); });
}

CHyprBar::~CHyprBar() {
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseButtonCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchDownCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchUpCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pTouchMoveCallback);
    HyprlandAPI::unregisterCallback(PHANDLE, m_pMouseMoveCallback);
    std::erase(g_pGlobalState->bars, m_self);
}

SDecorationPositioningInfo CHyprBar::getPositioningInfo() {
    static auto* const         PHEIGHT     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const         PENABLED    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();
    static auto* const         PPRECEDENCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();

    SDecorationPositioningInfo info;
    info.policy         = m_hidden ? DECORATION_POSITION_ABSOLUTE : DECORATION_POSITION_STICKY;
    info.edges          = DECORATION_EDGE_TOP;
    info.priority       = **PPRECEDENCE ? 10005 : 5000;
    info.reserved       = true;
    info.desiredExtents = {{0, m_hidden || !**PENABLED ? 0 : **PHEIGHT}, {0, 0}};
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

bool CHyprBar::inputIsValid() {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();

    if (!**PENABLED)
        return false;

    if (!m_pWindow->m_workspace || !m_pWindow->m_workspace->isVisible() || !g_pInputManager->m_exclusiveLSes.empty() ||
        (g_pSeatManager->m_seatGrab && !g_pSeatManager->m_seatGrab->accepts(m_pWindow->wlSurface()->resource())))
        return false;

    const auto WINDOWATCURSOR = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING);

    auto focusState = Desktop::focusState();
    auto window = focusState->window();
    auto monitor = focusState->monitor();
    
    if (WINDOWATCURSOR != m_pWindow && m_pWindow != window)
        return false;

    // check if input is on top or overlay shell layers
    auto     PMONITOR     = monitor;
    PHLLS    foundSurface = nullptr;
    Vector2D surfaceCoords;

    // check top layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], &surfaceCoords, &foundSurface);

    if (foundSurface)
        return false;
    // check overlay layer
    g_pCompositor->vectorToLayerSurface(g_pInputManager->getMouseCoordsInternal(), &PMONITOR->m_layerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], &surfaceCoords,
                                        &foundSurface);

    if (foundSurface)
        return false;

    return true;
}

void CHyprBar::onMouseButton(SCallbackInfo& info, IPointer::SButtonEvent e) {
    if (!inputIsValid())
        return;

    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED) {
        handleUpEvent(info);
        return;
    }

    handleDownEvent(info, std::nullopt);
}

void CHyprBar::onTouchDown(SCallbackInfo& info, ITouch::SDownEvent e) {
    // Don't do anything if you're already grabbed a window with another finger
    if (!inputIsValid() || e.touchID != 0)
        return;

    handleDownEvent(info, e);
}

void CHyprBar::onTouchUp(SCallbackInfo& info, ITouch::SUpEvent e) {
    if (!m_bDragPending || !m_bTouchEv || e.touchID != m_touchId)
        return;

    handleUpEvent(info);
}

void CHyprBar::onMouseMove(Vector2D coords) {
    // ensure proper redraws of button icons on hover when using hardware cursors
    static auto* const PICONONHOVER = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:icon_on_hover")->getDataStaticPtr();
    if (**PICONONHOVER)
        damageOnButtonHover();

    if (!m_bDragPending || m_bTouchEv || !validMapped(m_pWindow) || m_touchId != 0)
        return;

    m_bDragPending = false;
    handleMovement();
}

void CHyprBar::onTouchMove(SCallbackInfo& info, ITouch::SMotionEvent e) {
    if (!m_bDragPending || !m_bTouchEv || !validMapped(m_pWindow) || e.touchID != m_touchId)
        return;

    auto PMONITOR     = m_pWindow->m_monitor.lock();
    PMONITOR          = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();
    const auto COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y);

    if (!m_bDraggingThis) {
        // Initial setup for dragging a window.
        g_pKeybindManager->m_dispatchers["setfloating"]("activewindow");
        g_pKeybindManager->m_dispatchers["resizewindowpixel"]("exact 50% 50%,activewindow");
        // pin it so you can change workspaces while dragging a window
        g_pKeybindManager->m_dispatchers["pin"]("activewindow");
    }
    g_pKeybindManager->m_dispatchers["movewindowpixel"](std::format("exact {} {},activewindow", (int)(COORDS.x - (assignedBoxGlobal().w / 2)), (int)COORDS.y));
    m_bDraggingThis = true;
}

void CHyprBar::handleDownEvent(SCallbackInfo& info, std::optional<ITouch::SDownEvent> touchEvent) {
    m_bTouchEv = touchEvent.has_value();
    if (m_bTouchEv)
        m_touchId = touchEvent.value().touchID;

    const auto PWINDOW = m_pWindow.lock();

    auto       COORDS = cursorRelativeToBar();
    if (m_bTouchEv) {
        ITouch::SDownEvent e        = touchEvent.value();
        auto               PMONITOR = g_pCompositor->getMonitorFromName(!e.device->m_boundOutput.empty() ? e.device->m_boundOutput : "");
        PMONITOR                    = PMONITOR ? PMONITOR : Desktop::focusState()->monitor();
        COORDS = Vector2D(PMONITOR->m_position.x + e.pos.x * PMONITOR->m_size.x, PMONITOR->m_position.y + e.pos.y * PMONITOR->m_size.y) - assignedBoxGlobal().pos();
    }

    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PONDOUBLECLICK    = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:on_double_click")->getDataStaticPtr();

    const bool         BUTTONSRIGHT    = std::string{*PALIGNBUTTONS} != "left";
    const std::string  ON_DOUBLE_CLICK = *PONDOUBLECLICK;

    if (!VECINRECT(COORDS, 0, 0, assignedBoxGlobal().w, **PHEIGHT - 1)) {

        if (m_bDraggingThis) {
            if (m_bTouchEv)
                g_pKeybindManager->m_dispatchers["settiled"]("activewindow");
            g_pKeybindManager->m_dispatchers["mouse"]("0movewindow");
            Log::logger->log(Log::DEBUG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)PWINDOW.get());
        }

        m_bDraggingThis = false;
        m_bDragPending  = false;
        m_bTouchEv      = false;
        return;
    }

    if (Desktop::focusState()->window() != PWINDOW)
        Desktop::focusState()->fullWindowFocus(PWINDOW);

    if (PWINDOW->m_isFloating)
        g_pCompositor->changeWindowZOrder(PWINDOW, true);

    info.cancelled   = true;
    m_bCancelledDown = true;

    if (doButtonPress(PBARPADDING, PBARBUTTONPADDING, PHEIGHT, COORDS, BUTTONSRIGHT))
        return;

    if (!ON_DOUBLE_CLICK.empty() &&
        std::chrono::duration_cast<std::chrono::milliseconds>(Time::steadyNow() - m_lastMouseDown).count() < 400 /* Arbitrary delay I found suitable */) {
        g_pKeybindManager->m_dispatchers["exec"](ON_DOUBLE_CLICK);
        m_bDragPending = false;
    } else {
        m_lastMouseDown = Time::steadyNow();
        m_bDragPending  = true;
    }
}

void CHyprBar::handleUpEvent(SCallbackInfo& info) {
    if (m_pWindow.lock() != Desktop::focusState()->window())
        return;

    if (m_bCancelledDown)
        info.cancelled = true;

    m_bCancelledDown = false;

    if (m_bDraggingThis) {
        g_pKeybindManager->m_dispatchers["mouse"]("0movewindow");
        m_bDraggingThis = false;
        if (m_bTouchEv)
            g_pKeybindManager->m_dispatchers["settiled"]("activewindow");

        Log::logger->log(Log::DEBUG, "[hyprbars] Dragging ended on {:x}", (uintptr_t)m_pWindow.lock().get());
    }

    m_bDragPending = false;
    m_bTouchEv     = false;
    m_touchId      = 0;
}

void CHyprBar::handleMovement() {
    g_pKeybindManager->m_dispatchers["mouse"]("1movewindow");
    m_bDraggingThis = true;
    Log::logger->log(Log::DEBUG, "[hyprbars] Dragging initiated on {:x}", (uintptr_t)m_pWindow.lock().get());
    return;
}

bool CHyprBar::doButtonPress(Hyprlang::INT* const* PBARPADDING, Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PHEIGHT, Vector2D COORDS, const bool BUTTONSRIGHT) {
    //check if on a button
    float offset = **PBARPADDING;

    for (auto& b : g_pGlobalState->buttons) {
        const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, **PHEIGHT};
        Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - **PBARBUTTONPADDING - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

        if (VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + **PBARBUTTONPADDING, currentPos.y + b.size)) {
            // hit on close
            g_pKeybindManager->m_dispatchers["exec"](b.cmd);
            return true;
        }

        offset += **PBARBUTTONPADDING + b.size;
    }
    return false;
}

void CHyprBar::renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize) {
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

    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);

    const int    layoutWidth  = ink_rect.width;
    const int    layoutHeight = logical_rect.height;

    const double xOffset = (bufferSize.x / 2.0 - layoutWidth / PANGO_SCALE / 2.0);
    const double yOffset = (bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0);

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    out->allocate();
    glBindTexture(GL_TEXTURE_2D, out->m_texID);
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

    const auto       scaledSize        = **PSIZE * scale;
    const auto       scaledBorderSize  = BORDERSIZE * scale;
    const auto       scaledButtonsSize = buttonSizes * scale;
    const auto       scaledButtonsPad  = **PBARBUTTONPADDING * scale;
    const auto       scaledBarPadding  = **PBARPADDING * scale;

    const CHyprColor COLOR = m_bForcedTitleColor.value_or(**PCOLOR);

    const auto       CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto       CAIRO        = cairo_create(CAIROSURFACE);

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

    PangoContext* context = pango_layout_get_context(layout);
    pango_context_set_base_dir(context, PANGO_DIRECTION_NEUTRAL);

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
    glBindTexture(GL_TEXTURE_2D, m_pTextTex->m_texID);
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

size_t CHyprBar::getVisibleButtonCount(Hyprlang::INT* const* PBARBUTTONPADDING, Hyprlang::INT* const* PBARPADDING, const Vector2D& bufferSize, const float scale) {
    float  availableSpace = bufferSize.x - **PBARPADDING * scale * 2;
    size_t count          = 0;

    for (const auto& button : g_pGlobalState->buttons) {
        const float buttonSpace = (button.size + **PBARBUTTONPADDING) * scale;
        if (availableSpace >= buttonSpace) {
            count++;
            availableSpace -= buttonSpace;
        } else
            break;
    }

    return count;
}

void CHyprBar::renderBarButtons(const Vector2D& bufferSize, const float scale) {
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PINACTIVECOLOR    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";
    const auto         visibleCount = getVisibleButtonCount(PBARBUTTONPADDING, PBARPADDING, bufferSize, scale);

    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw buttons
    int offset = **PBARPADDING * scale;
    for (size_t i = 0; i < visibleCount; ++i) {
        const auto& button           = g_pGlobalState->buttons[i];
        const auto  scaledButtonSize = button.size * scale;
        const auto  scaledButtonsPad = **PBARBUTTONPADDING * scale;

        const auto  pos   = Vector2D{BUTTONSRIGHT ? bufferSize.x - offset - scaledButtonSize / 2.0 : offset + scaledButtonSize / 2.0, bufferSize.y / 2.0}.floor();
        auto        color = button.bgcol;

        if (**PINACTIVECOLOR > 0) {
            color = m_bWindowHasFocus ? color : CHyprColor(**PINACTIVECOLOR);
            if (button.userfg && button.iconTex->m_texID != 0)
                button.iconTex->destroyTexture();
        }

        cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);
        cairo_arc(CAIRO, pos.x, pos.y, scaledButtonSize / 2, 0, 2 * M_PI);
        cairo_fill(CAIRO);

        offset += scaledButtonsPad + scaledButtonSize;
    }

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    m_pButtonsTex->allocate();
    glBindTexture(GL_TEXTURE_2D, m_pButtonsTex->m_texID);
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
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PICONONHOVER      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:icon_on_hover")->getDataStaticPtr();

    const bool         BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";
    const auto         visibleCount = getVisibleButtonCount(PBARBUTTONPADDING, PBARPADDING, Vector2D{barBox->w, barBox->h}, scale);
    const auto         COORDS       = cursorRelativeToBar();

    int                offset        = **PBARPADDING * scale;
    float              noScaleOffset = **PBARPADDING;

    for (size_t i = 0; i < visibleCount; ++i) {
        auto&      button           = g_pGlobalState->buttons[i];
        const auto scaledButtonSize = button.size * scale;
        const auto scaledButtonsPad = **PBARBUTTONPADDING * scale;

        // check if hovering here
        const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, **PHEIGHT};
        Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - **PBARBUTTONPADDING - button.size - noScaleOffset : noScaleOffset), (BARBUF.y - button.size) / 2.0}.floor();
        bool       hovering   = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + button.size + **PBARBUTTONPADDING, currentPos.y + button.size);
        noScaleOffset += **PBARBUTTONPADDING + button.size;

        if (button.iconTex->m_texID == 0 /* icon is not rendered */ && !button.icon.empty()) {
            // render icon
            const Vector2D BUFSIZE = {scaledButtonSize, scaledButtonSize};
            auto           fgcol   = button.userfg ? button.fgcol : (button.bgcol.r + button.bgcol.g + button.bgcol.b < 1) ? CHyprColor(0xFFFFFFFF) : CHyprColor(0xFF000000);

            renderText(button.iconTex, button.icon, fgcol, BUFSIZE, scale, button.size * 0.62);
        }

        if (button.iconTex->m_texID == 0)
            continue;

        CBox pos = {barBox->x + (BUTTONSRIGHT ? barBox->width - offset - scaledButtonSize : offset), barBox->y + (barBox->height - scaledButtonSize) / 2.0, scaledButtonSize,
                    scaledButtonSize};

        if (!**PICONONHOVER || (**PICONONHOVER && m_iButtonHoverState > 0))
            g_pHyprOpenGL->renderTexture(button.iconTex, pos, {.a = a});
        offset += scaledButtonsPad + scaledButtonSize;

        bool currentBit = (m_iButtonHoverState & (1 << i)) != 0;
        if (hovering != currentBit) {
            m_iButtonHoverState ^= (1 << i);
            // damage to get rid of some artifacts when icons are "hidden"
            damageEntire();
        }
    }
}

void CHyprBar::draw(PHLMONITOR pMonitor, const float& a) {
    static auto* const PENABLED = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:enabled")->getDataStaticPtr();

    if (m_bLastEnabledState != **PENABLED) {
        m_bLastEnabledState = **PENABLED;
        g_pDecorationPositioner->repositionDeco(this);
    }

    if (m_hidden || !validMapped(m_pWindow) || !**PENABLED)
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_ruleApplicator->decorate().valueOrDefault())
        return;

    auto data = CBarPassElement::SBarData{this, a};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBarPassElement>(data));
}

void CHyprBar::renderPass(PHLMONITOR pMonitor, const float& a) {
    const auto         PWINDOW = m_pWindow.lock();

    static auto* const PCOLOR            = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_color")->getDataStaticPtr();
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PPRECEDENCE       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    static auto* const PENABLETITLE      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled")->getDataStaticPtr();
    static auto* const PENABLEBLUR       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_blur")->getDataStaticPtr();
    static auto* const PENABLEBLURGLOBAL = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "decoration:blur:enabled")->getDataStaticPtr();
    static auto* const PINACTIVECOLOR    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:inactive_button_color")->getDataStaticPtr();

    if (**PINACTIVECOLOR > 0) {
        bool currentWindowFocus = PWINDOW == Desktop::focusState()->window();
        if (currentWindowFocus != m_bWindowHasFocus) {
            m_bWindowHasFocus = currentWindowFocus;
            m_bButtonsDirty   = true;
        }
    }

    const CHyprColor DEST_COLOR = m_bForcedBarColor.value_or(**PCOLOR);
    if (DEST_COLOR != m_cRealBarColor->goal())
        *m_cRealBarColor = DEST_COLOR;

    CHyprColor color = m_cRealBarColor->value();

    color.a *= a;
    const bool BUTTONSRIGHT = std::string{*PALIGNBUTTONS} != "left";
    const bool SHOULDBLUR   = **PENABLEBLUR && **PENABLEBLURGLOBAL && color.a < 1.F;

    if (**PHEIGHT < 1) {
        m_iLastHeight = **PHEIGHT;
        return;
    }

    const auto PWORKSPACE      = PWINDOW->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !PWINDOW->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    const auto ROUNDING = PWINDOW->rounding() + (*PPRECEDENCE ? 0 : PWINDOW->getRealBorderSize());

    const auto scaledRounding = ROUNDING > 0 ? ROUNDING * pMonitor->m_scale - 2 /* idk why but otherwise it looks bad due to the gaps */ : 0;

    m_seExtents = {{0, **PHEIGHT}, {}};

    const auto DECOBOX = assignedBoxGlobal();

    const auto BARBUF = DECOBOX.size() * pMonitor->m_scale;

    CBox       titleBarBox = {DECOBOX.x - pMonitor->m_position.x, DECOBOX.y - pMonitor->m_position.y, DECOBOX.w,
                              DECOBOX.h + ROUNDING * 3 /* to fill the bottom cuz we can't disable rounding there */};

    titleBarBox.translate(PWINDOW->m_floatingOffset).scale(pMonitor->m_scale).round();

    if (titleBarBox.w < 1 || titleBarBox.h < 1)
        return;

    g_pHyprOpenGL->scissor(titleBarBox);

    if (ROUNDING) {
        // the +1 is a shit garbage temp fix until renderRect supports an alpha matte
        CBox windowBox = {PWINDOW->m_realPosition->value().x + PWINDOW->m_floatingOffset.x - pMonitor->m_position.x + 1,
                          PWINDOW->m_realPosition->value().y + PWINDOW->m_floatingOffset.y - pMonitor->m_position.y + 1, PWINDOW->m_realSize->value().x - 2,
                          PWINDOW->m_realSize->value().y - 2};

        if (windowBox.w < 1 || windowBox.h < 1)
            return;

        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, true);

        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        windowBox.translate(WORKSPACEOFFSET).scale(pMonitor->m_scale).round();
        g_pHyprOpenGL->renderRect(windowBox, CHyprColor(0, 0, 0, 0), {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower()});
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glStencilFunc(GL_NOTEQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    if (SHOULDBLUR)
        g_pHyprOpenGL->renderRect(titleBarBox, color, {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower(), .blur = true, .blurA = a});
    else
        g_pHyprOpenGL->renderRect(titleBarBox, color, {.round = scaledRounding, .roundingPower = m_pWindow->roundingPower()});

    // render title
    if (**PENABLETITLE && (m_szLastTitle != PWINDOW->m_title || m_bWindowSizeChanged || m_pTextTex->m_texID == 0 || m_bTitleColorChanged)) {
        m_szLastTitle = PWINDOW->m_title;
        renderBarTitle(BARBUF, pMonitor->m_scale);
    }

    if (ROUNDING) {
        // cleanup stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        g_pHyprOpenGL->setCapStatus(GL_STENCIL_TEST, false);
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    }

    CBox textBox = {titleBarBox.x, titleBarBox.y, (int)BARBUF.x, (int)BARBUF.y};
    if (**PENABLETITLE)
        g_pHyprOpenGL->renderTexture(m_pTextTex, textBox, {.a = a});

    if (m_bButtonsDirty || m_bWindowSizeChanged) {
        renderBarButtons(BARBUF, pMonitor->m_scale);
        m_bButtonsDirty = false;
    }

    g_pHyprOpenGL->renderTexture(m_pButtonsTex, textBox, {.a = a});

    g_pHyprOpenGL->scissor(nullptr);

    renderBarButtonsText(&textBox, pMonitor->m_scale, a);

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
    g_pHyprRenderer->damageBox(assignedBoxGlobal());
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
    if (!validMapped(m_pWindow))
        return {};

    CBox box = m_bAssignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_pWindow.lock()));

    const auto PWORKSPACE      = m_pWindow->m_workspace;
    const auto WORKSPACEOFFSET = PWORKSPACE && !m_pWindow->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    return box.translate(WORKSPACEOFFSET);
}

PHLWINDOW CHyprBar::getOwner() {
    return m_pWindow.lock();
}

void CHyprBar::updateRules() {
    const auto PWINDOW              = m_pWindow.lock();
    auto       prevHidden           = m_hidden;
    auto       prevForcedTitleColor = m_bForcedTitleColor;

    m_bForcedBarColor   = std::nullopt;
    m_bForcedTitleColor = std::nullopt;
    m_hidden            = false;

    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->nobarRuleIdx))
        m_hidden = truthy(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_pGlobalState->nobarRuleIdx)->effect);
    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->barColorRuleIdx))
        m_bForcedBarColor = CHyprColor(configStringToInt(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_pGlobalState->barColorRuleIdx)->effect).value_or(0));
    if (PWINDOW->m_ruleApplicator->m_otherProps.props.contains(g_pGlobalState->titleColorRuleIdx))
        m_bForcedTitleColor = CHyprColor(configStringToInt(PWINDOW->m_ruleApplicator->m_otherProps.props.at(g_pGlobalState->titleColorRuleIdx)->effect).value_or(0));

    if (prevHidden != m_hidden)
        g_pDecorationPositioner->repositionDeco(this);
    if (prevForcedTitleColor != m_bForcedTitleColor)
        m_bTitleColorChanged = true;
}

void CHyprBar::damageOnButtonHover() {
    static auto* const PBARPADDING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_padding")->getDataStaticPtr();
    static auto* const PBARBUTTONPADDING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding")->getDataStaticPtr();
    static auto* const PHEIGHT           = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_height")->getDataStaticPtr();
    static auto* const PALIGNBUTTONS     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment")->getDataStaticPtr();
    const bool         BUTTONSRIGHT      = std::string{*PALIGNBUTTONS} != "left";

    float              offset = **PBARPADDING;

    const auto         COORDS = cursorRelativeToBar();

    for (auto& b : g_pGlobalState->buttons) {
        const auto BARBUF     = Vector2D{(int)assignedBoxGlobal().w, **PHEIGHT};
        Vector2D   currentPos = Vector2D{(BUTTONSRIGHT ? BARBUF.x - **PBARBUTTONPADDING - b.size - offset : offset), (BARBUF.y - b.size) / 2.0}.floor();

        bool       hover = VECINRECT(COORDS, currentPos.x, currentPos.y, currentPos.x + b.size + **PBARBUTTONPADDING, currentPos.y + b.size);

        if (hover != m_bButtonHovered) {
            m_bButtonHovered = hover;
            damageEntire();
        }

        offset += **PBARBUTTONPADDING + b.size;
    }
}
