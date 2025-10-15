#include "overview.hpp"
#include <any>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <pango/pangocairo.h>
#include <cmath>

struct SHyprGradientSpec {
    CHyprColor c1;
    CHyprColor c2;
    float      angleDeg = 0.f;
    bool       valid    = false;
};

static bool parseHexRGBA8(const std::string& s, CHyprColor& out) {
    // expects 8 hex digits RRGGBBAA
    if (s.size() != 8)
        return false;
    auto hexTo = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    auto byteAt = [&](int i) -> int {
        int a = hexTo(s[i]);
        int b = hexTo(s[i+1]);
        if (a < 0 || b < 0) return -1;
        return (a << 4) | b;
    };
    int r = byteAt(0);
    int g = byteAt(2);
    int b = byteAt(4);
    int a = byteAt(6);
    if (r < 0 || g < 0 || b < 0 || a < 0) return false;
    out = CHyprColor{r / 255.f, g / 255.f, b / 255.f, a / 255.f};
    return true;
}

static SHyprGradientSpec parseGradientSpec(const std::string& inRaw) {
    // Accept forms like: "rgba(33ccffee) rgba(00ff99ee) 45deg"
    // Extract 8 hex digits from two rgba(...) groups and an integer angle
    SHyprGradientSpec spec;
    std::string       s = inRaw;
    // remove commas
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    // find 1st rgba(XXXXXXXX)
    auto p1 = s.find("rgba(");
    auto p2 = s.find("rgba(", p1 == std::string::npos ? 0 : p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos)
        return spec;
    auto e1 = s.find(')', p1);
    auto e2 = s.find(')', p2);
    if (e1 == std::string::npos || e2 == std::string::npos)
        return spec;
    const std::string hex1 = s.substr(p1 + 5, e1 - (p1 + 5));
    const std::string hex2 = s.substr(p2 + 5, e2 - (p2 + 5));
    CHyprColor        c1, c2;
    if (!parseHexRGBA8(hex1, c1) || !parseHexRGBA8(hex2, c2))
        return spec;
    // find angle
    float angle = 0.f;
    auto  pd    = s.find("deg", e2);
    if (pd != std::string::npos) {
        // collect digits before 'deg'
        size_t beg = s.rfind(' ', pd);
        if (beg == std::string::npos)
            beg = e2 + 1;
        try {
            angle = std::stof(s.substr(beg, pd - beg));
        } catch (...) { angle = 0.f; }
    }
    spec.c1       = c1;
    spec.c2       = c2;
    spec.angleDeg = angle;
    spec.valid    = true;
    return spec;
}

// Helper to detect if a border config string is a gradient or solid color
static bool isGradientBorderSpec(const std::string& borderSpec) {
    if (borderSpec.empty())
        return false;
    // Check if it contains gradient pattern: rgba(...) rgba(...) deg
    return borderSpec.find("rgba(") != std::string::npos &&
           borderSpec.rfind("rgba(") != borderSpec.find("rgba(");
}

static void renderGradientBorder(const CBox& box, int borderSize, const SHyprGradientSpec& grad, int round = 0) {
    if (!grad.valid || borderSize <= 0)
        return;

    // gradient direction
    const float  rad = grad.angleDeg * (float)M_PI / 180.f;
    const Vector2D g{std::cos(rad), std::sin(rad)};
    // compute min/max dot among corners
    const Vector2D corners[4] = {{box.x, box.y}, {box.x + box.w, box.y}, {box.x, box.y + box.h}, {box.x + box.w, box.y + box.h}};
    float         minD        = 1e9f, maxD = -1e9f;
    for (auto& c : corners) {
        float d = c.x * g.x + c.y * g.y;
        minD    = std::min(minD, d);
        maxD    = std::max(maxD, d);
    }
    const float range = std::max(1e-3f, maxD - minD);

    auto mixCol = [](const CHyprColor& a, const CHyprColor& b, float t) {
        t   = std::clamp(t, 0.f, 1.f);
        auto m = CHyprColor{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
        return m;
    };

    // choose segment counts
    const int segW = std::clamp((int)std::round(box.w / 20.0), 8, 64);
    const int segH = std::clamp((int)std::round(box.h / 20.0), 8, 64);

    auto drawSeg = [&](const CBox& r) {
        const float cx = r.x + r.w / 2.0;
        const float cy = r.y + r.h / 2.0;
        const float d  = cx * g.x + cy * g.y;
        const float t  = (d - minD) / range;
        g_pHyprOpenGL->renderRect(r, mixCol(grad.c1, grad.c2, t), {});
    };

    const double cr = std::clamp((double)round, 0.0, std::min(box.w, box.h) / 2.0);

    // top and bottom bars (shrink horizontally by cr)
    if (box.w > 2 * cr) {
        for (int i = 0; i < segW; ++i) {
            const double sx = box.x + cr + (double)i * ((box.w - 2 * cr) / segW);
            const double sw = (i == segW - 1) ? (box.x + box.w - cr - sx) : ((box.w - 2 * cr) / segW);
            drawSeg(CBox{sx, box.y, sw, (double)borderSize});
            drawSeg(CBox{sx, box.y + box.h - borderSize, sw, (double)borderSize});
        }
    }

    // left and right bars (shrink vertically by cr)
    if (box.h > 2 * cr) {
        for (int i = 0; i < segH; ++i) {
            const double sy = box.y + cr + (double)i * ((box.h - 2 * cr) / segH);
            const double sh = (i == segH - 1) ? (box.y + box.h - cr - sy) : ((box.h - 2 * cr) / segH);
            drawSeg(CBox{box.x, sy, (double)borderSize, sh});
            drawSeg(CBox{box.x + box.w - borderSize, sy, (double)borderSize, sh});
        }
    }
}

static void renderNumberTexture(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize) {
    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, text.c_str(), -1);

    // font options from config
    static auto* const PFONTFAM = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_font_family")->getDataStaticPtr();
    static auto* const PFONTB   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_font_bold")->getDataStaticPtr();
    static auto* const PFONTI   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_font_italic")->getDataStaticPtr();
    static auto* const PTUNDER  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_text_underline")->getDataStaticPtr();
    static auto* const PTSTRIKE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_text_strikethrough")->getDataStaticPtr();

    PangoFontDescription* fontDesc = pango_font_description_from_string(*PFONTFAM);
    pango_font_description_set_size(fontDesc, fontSize * scale * PANGO_SCALE);
    pango_font_description_set_weight(fontDesc, **PFONTB ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(fontDesc, **PFONTI ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    if (**PTUNDER || **PTSTRIKE) {
        PangoAttrList* attrs = pango_attr_list_new();
        if (**PTUNDER) {
            pango_attr_list_insert(attrs, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
        }
        if (**PTSTRIKE) {
            pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
        }
        pango_layout_set_attributes(layout, attrs);
        pango_attr_list_unref(attrs);
    }

    pango_layout_set_width(layout, bufferSize.x * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);

    cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);

    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);

    // center inside the provided buffer using ink rect (accounts for glyph bearings)
    const int    inkW   = std::max(0, ink_rect.width / PANGO_SCALE);
    const int    inkH   = std::max(0, ink_rect.height / PANGO_SCALE);
    const int    inkX   = ink_rect.x / PANGO_SCALE; // can be negative
    const int    inkY   = ink_rect.y / PANGO_SCALE; // can be negative
    static auto* const* PCENTERADJX = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_center_adjust_x")->getDataStaticPtr();
    static auto* const* PCENTERADJY = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_center_adjust_y")->getDataStaticPtr();
    const double xOffset = (bufferSize.x - inkW) / 2.0 - inkX + **PCENTERADJX;
    const double yOffset = (bufferSize.y - inkH) / 2.0 - inkY + **PCENTERADJY;

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);
    g_object_unref(layout);

    cairo_surface_flush(CAIROSURFACE);

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

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

static void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview.reset();
}

// Get workspace method configuration for a specific monitor
// Returns pair of {isCenter, startWorkspaceID}
static std::pair<bool, int> getWorkspaceMethodForMonitor(PHLMONITOR monitor) {
    static auto const* PMETHOD = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method")->getDataStaticPtr();

    // Check for monitor-specific override first (from hyprexpo_workspace_method keyword)
    const std::string monitorName = monitor->m_name;
    std::string methodStr;

    auto it = g_monitorWorkspaceMethods.find(monitorName);
    if (it != g_monitorWorkspaceMethods.end()) {
        // Use monitor-specific config
        methodStr = it->second;
    } else {
        // Fallback to global plugin config
        methodStr = std::string{*PMETHOD};
    }

    // Parse the method string
    bool methodCenter = true;
    int methodStartID = monitor->activeWorkspaceID();

    CVarList method{methodStr, 0, 's', true};
    if (method.size() < 2) {
        Debug::log(ERR, "[he] invalid workspace_method for monitor {}: {}", monitorName, methodStr);
    } else {
        methodCenter = method[0] == "center";
        methodStartID = getWorkspaceIDNameFromString(method[1]).id;
        if (methodStartID == WORKSPACE_INVALID)
            methodStartID = monitor->activeWorkspaceID();
    }

    return {methodCenter, methodStartID};
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pInputManager->unsetCursorImage();
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
    resetSubmapIfNeeded();
}

COverview::COverview(PHLWORKSPACE startedOn_, bool swipe_) : startedOn(startedOn_), swipe(swipe_) {
    const auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
    pMonitor            = PMONITOR;

    static auto* const* PCOLUMNS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:columns")->getDataStaticPtr();
    static auto* const* PGAPS    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gaps_in")->getDataStaticPtr();
    static auto* const* PCOL     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:bg_col")->getDataStaticPtr();
    static auto* const* PSKIP    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:skip_empty")->getDataStaticPtr();

    SIDE_LENGTH = **PCOLUMNS;
    GAP_WIDTH   = **PGAPS;
    BG_COLOR    = **PCOL;

    // Get workspace method for this specific monitor
    auto [methodCenter, methodStartID] = getWorkspaceMethodForMonitor(pMonitor.lock());

    images.resize(SIDE_LENGTH * SIDE_LENGTH);

    // r includes empty workspaces; m skips over them
    std::string selector = **PSKIP ? "m" : "r";

    if (methodCenter) {
        int currentID = methodStartID;
        int firstID   = currentID;

        int backtracked = 0;

        // Initialize tiles to WORKSPACE_INVALID; cliking one of these results
        // in changing to "emptynm" (next empty workspace). Tiles with this id
        // will only remain if skip_empty is on.
        for (size_t i = 0; i < images.size(); i++) {
            images[i].workspaceID = WORKSPACE_INVALID;
        }

        // Scan through workspaces lower than methodStartID until we wrap; count how many
        for (size_t i = 1; i < images.size() / 2; ++i) {
            currentID = getWorkspaceIDNameFromString(selector + "-" + std::to_string(i)).id;
            if (currentID >= firstID)
                break;

            backtracked++;
            firstID = currentID;
        }

        // Scan through workspaces higher than methodStartID. If using "m"
        // (skip_empty), stop when we wrap, leaving the rest of the workspace
        // ID's set to WORKSPACE_INVALID
        for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
            auto& image = images[i];
            if ((int64_t)i - backtracked < 0) {
                currentID = getWorkspaceIDNameFromString(selector + std::to_string((int64_t)i - backtracked)).id;
            } else {
                currentID = getWorkspaceIDNameFromString(selector + "+" + std::to_string((int64_t)i - backtracked)).id;
                if (i > 0 && currentID == firstID)
                    break;
            }
            image.workspaceID = currentID;
        }

    } else {
        int currentID         = methodStartID;
        images[0].workspaceID = currentID;

        auto PWORKSPACESTART = g_pCompositor->getWorkspaceByID(currentID);
        if (!PWORKSPACESTART)
            PWORKSPACESTART = CWorkspace::create(currentID, pMonitor.lock(), std::to_string(currentID));

        pMonitor->m_activeWorkspace = PWORKSPACESTART;

        // Scan through workspaces higher than methodStartID. If using "m"
        // (skip_empty), stop when we wrap, leaving the rest of the workspace
        // ID's set to WORKSPACE_INVALID
        for (size_t i = 1; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
            auto& image = images[i];
            currentID   = getWorkspaceIDNameFromString(selector + "+" + std::to_string(i)).id;
            if (currentID <= methodStartID)
                break;
            image.workspaceID = currentID;
        }

        pMonitor->m_activeWorkspace = startedOn;
    }

    g_pHyprRenderer->makeEGLCurrent();

    Vector2D tileSize       = pMonitor->m_size / SIDE_LENGTH;
    Vector2D tileRenderSize = (pMonitor->m_size - Vector2D{GAP_WIDTH * pMonitor->m_scale, GAP_WIDTH * pMonitor->m_scale} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    int          currentid = 0;

    PHLWORKSPACE openSpecial = PMONITOR->m_activeSpecialWorkspace;
    if (openSpecial)
        PMONITOR->m_activeSpecialWorkspace.reset();

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

    startedOn->m_visible = false;

    for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
        COverview::SWorkspaceImage& image = images[i];
        image.fb.alloc(monbox.w, monbox.h, PMONITOR->m_output->state->state().drmFormat);

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(image.workspaceID);

        if (PWORKSPACE == startedOn)
            currentid = i;

        if (PWORKSPACE) {
            image.pWorkspace            = PWORKSPACE;
            PMONITOR->m_activeWorkspace = PWORKSPACE;
            g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
            PWORKSPACE->m_visible = true;

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace = openSpecial;

            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, Time::steadyNow(), monbox);

            PWORKSPACE->m_visible = false;
            g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

            if (PWORKSPACE == startedOn)
                PMONITOR->m_activeSpecialWorkspace.reset();
        } else
            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, Time::steadyNow(), monbox);

        image.box = {(i % SIDE_LENGTH) * tileRenderSize.x + (i % SIDE_LENGTH) * GAP_WIDTH, (i / SIDE_LENGTH) * tileRenderSize.y + (i / SIDE_LENGTH) * GAP_WIDTH, tileRenderSize.x,
                     tileRenderSize.y};

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    PMONITOR->m_activeSpecialWorkspace = openSpecial;
    PMONITOR->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    // zoom on the current workspace.
    // const auto& TILE = images[std::clamp(currentid, 0, SIDE_LENGTH * SIDE_LENGTH)];

    g_pAnimationManager->createAnimation(pMonitor->m_size * pMonitor->m_size / tileSize, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation((-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{currentid % SIDE_LENGTH, currentid / SIDE_LENGTH}) * pMonitor->m_scale) *
                                             (pMonitor->m_size / tileSize),
                                         pos, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    size->setUpdateCallback(damageMonitor);
    pos->setUpdateCallback(damageMonitor);

    if (!swipe) {
        *size = pMonitor->m_size;
        *pos  = {0, 0};

        size->setCallbackOnEnd([this](auto) { redrawAll(true); });
    }

    openedID = currentid;

    g_pInputManager->setCursorImageUntilUnset("left_ptr");

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    // Initialize hoveredID based on current mouse position
    int hx = std::clamp((int)(lastMousePosLocal.x / pMonitor->m_size.x * SIDE_LENGTH), 0, SIDE_LENGTH - 1);
    int hy = std::clamp((int)(lastMousePosLocal.y / pMonitor->m_size.y * SIDE_LENGTH), 0, SIDE_LENGTH - 1);
    hoveredID = hx + hy * SIDE_LENGTH;

    auto onCursorMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

        // Update hovered tile
        int hx = std::clamp((int)(lastMousePosLocal.x / pMonitor->m_size.x * SIDE_LENGTH), 0, SIDE_LENGTH - 1);
        int hy = std::clamp((int)(lastMousePosLocal.y / pMonitor->m_size.y * SIDE_LENGTH), 0, SIDE_LENGTH - 1);
        int newHoveredID = hx + hy * SIDE_LENGTH;

        if (newHoveredID != hoveredID) {
            hoveredID = newHoveredID;
            damage();
        }
    };

    auto onCursorSelect = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        selectHoveredWorkspace();

        close();
    };

    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onCursorMove);
    touchMoveHook = g_pHookSystem->hookDynamic("touchMove", onCursorMove);

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onCursorSelect);
    touchDownHook   = g_pHookSystem->hookDynamic("touchDown", onCursorSelect);

    enterSubmapIfEnabled();
}

void COverview::selectHoveredWorkspace() {
    if (closing)
        return;

    // get tile x,y
    int x     = lastMousePosLocal.x / pMonitor->m_size.x * SIDE_LENGTH;
    int y     = lastMousePosLocal.y / pMonitor->m_size.y * SIDE_LENGTH;
    closeOnID = x + y * SIDE_LENGTH;
}

void COverview::ensureKbFocusInitialized() {
    if (kbFocusID != -1)
        return;

    // try to set to current openedID
    if (openedID != -1) {
        kbFocusID = openedID;
        return;
    }

    // fallback: first valid tile
    for (size_t i = 0; i < images.size(); ++i) {
        if (isTileValid(i)) {
            kbFocusID = i;
            return;
        }
    }
}

bool COverview::isTileValid(int id) const {
    if (id < 0 || id >= SIDE_LENGTH * SIDE_LENGTH)
        return false;
    return images[id].workspaceID != WORKSPACE_INVALID;
}

int COverview::tileForWorkspaceID(int wsid) const {
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID == wsid)
            return (int)i;
    }
    return -1;
}

int COverview::tileForVisibleIndex(int vIdx) const {
    if (vIdx < 0)
        return -1;
    int seen = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID == WORKSPACE_INVALID)
            continue;
        if (seen == vIdx)
            return (int)i;
        ++seen;
    }
    return -1;
}

void COverview::moveFocus(int dx, int dy) {
    ensureKbFocusInitialized();
    if (kbFocusID == -1)
        return;

    int x = kbFocusID % SIDE_LENGTH;
    int y = kbFocusID / SIDE_LENGTH;

    static auto* const* PWRAPH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_h")->getDataStaticPtr();
    static auto* const* PWRAPV = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_v")->getDataStaticPtr();

    if (dx != 0) {
        static auto* const* PREADING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_reading_order")->getDataStaticPtr();
        int                 step     = dx > 0 ? 1 : -1;
        if (**PREADING) {
            // reading-order scan: proceed linearly across the grid (row-major)
            const int total = SIDE_LENGTH * SIDE_LENGTH;
            int       idx   = kbFocusID;
            for (int tries = 0; tries < total; ++tries) {
                idx += step;
                if (idx < 0 || idx >= total) {
                    // wrap only if both wraps are enabled (edge of grid)
                    if (**PWRAPH && **PWRAPV)
                        idx = (idx + total) % total;
                    else
                        break;
                }
                if (isTileValid(idx)) {
                    kbFocusID = idx;
                    return;
                }
            }
        } else {
            // in-row scan with optional horizontal wrap
            int nx = x;
            for (int tries = 0; tries < SIDE_LENGTH; ++tries) {
                nx += step;
                if (nx < 0 || nx >= SIDE_LENGTH) {
                    if (**PWRAPH)
                        nx = (nx + SIDE_LENGTH) % SIDE_LENGTH;
                    else
                        break;
                }
                const int nid = nx + y * SIDE_LENGTH;
                if (isTileValid(nid)) {
                    kbFocusID = nid;
                    return;
                }
            }
        }
    }

    if (dy != 0) {
        int step = dy > 0 ? 1 : -1;
        int ny   = y;
        for (int tries = 0; tries < SIDE_LENGTH; ++tries) {
            ny += step;
            if (ny < 0 || ny >= SIDE_LENGTH) {
                if (**PWRAPV)
                    ny = (ny + SIDE_LENGTH) % SIDE_LENGTH;
                else
                    break;
            }
            const int nid = x + ny * SIDE_LENGTH;
            if (isTileValid(nid)) {
                kbFocusID = nid;
                return;
            }
        }
    }
}

void COverview::onKbMoveFocus(const std::string& dir) {
    if (closing)
        return;
    if (dir == "left")
        moveFocus(-1, 0);
    else if (dir == "right")
        moveFocus(1, 0);
    else if (dir == "up")
        moveFocus(0, -1);
    else if (dir == "down")
        moveFocus(0, 1);

    damage();
}

void COverview::onKbConfirm() {
    if (closing)
        return;
    ensureKbFocusInitialized();
    if (kbFocusID != -1)
        closeOnID = kbFocusID;
    close();
}

void COverview::onKbSelectNumber(int num) {
    if (closing)
        return;

    if (num == 0)
        num = 10;

    const int tid = tileForWorkspaceID(num);
    if (tid != -1) {
        closeOnID = tid;
        close();
    }
}

void COverview::onKbSelectToken(int visibleIdx) {
    if (closing)
        return;
    if (visibleIdx < 0)
        return;
    const int tid = tileForVisibleIndex(visibleIdx);
    if (tid != -1) {
        closeOnID = tid;
        close();
    }
}

void COverview::redrawID(int id, bool forcelowres) {
    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    id = std::clamp(id, 0, SIDE_LENGTH * SIDE_LENGTH);

    Vector2D tileSize       = pMonitor->m_size / SIDE_LENGTH;
    Vector2D tileRenderSize = (pMonitor->m_size - Vector2D{GAP_WIDTH, GAP_WIDTH} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!forcelowres && (size->value() != pMonitor->m_size || closing))
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    auto& image = images[id];

    if (image.fb.m_size != monbox.size()) {
        image.fb.release();
        image.fb.alloc(monbox.w, monbox.h, pMonitor->m_output->state->state().drmFormat);
    }

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    const auto   PWORKSPACE = image.pWorkspace;

    PHLWORKSPACE openSpecial = pMonitor->m_activeSpecialWorkspace;
    if (openSpecial)
        pMonitor->m_activeSpecialWorkspace.reset();

    startedOn->m_visible = false;

    if (PWORKSPACE) {
        pMonitor->m_activeWorkspace = PWORKSPACE;
        g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        PWORKSPACE->m_visible = true;

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace = openSpecial;

        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

        PWORKSPACE->m_visible = false;
        g_pDesktopAnimationManager->startAnimation(PWORKSPACE, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

        if (PWORKSPACE == startedOn)
            pMonitor->m_activeSpecialWorkspace.reset();
    } else
        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, Time::steadyNow(), monbox);

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    pMonitor->m_activeSpecialWorkspace = openSpecial;
    pMonitor->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    blockOverviewRendering = false;
}

void COverview::redrawAll(bool forcelowres) {
    for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
        redrawID(i, forcelowres);
    }
}

void COverview::damage() {
    blockDamageReporting = true;
    g_pHyprRenderer->damageMonitor(pMonitor.lock());
    blockDamageReporting = false;
}

void COverview::onDamageReported() {
    damageDirty = true;

    Vector2D SIZE = size->value();

    Vector2D tileSize       = (SIZE / SIDE_LENGTH);
    const auto GAPSIZE      = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * GAP_WIDTH;
    static auto* const* PGAPSO = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gaps_out")->getDataStaticPtr();
    const float OUTER       = **PGAPSO * (closing ? (1.0 - size->getPercent()) : size->getPercent());
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (SIDE_LENGTH - 1) - Vector2D{OUTER * 2, OUTER * 2}) / SIDE_LENGTH;
    // const auto& TILE           = images[std::clamp(openedID, 0, SIDE_LENGTH * SIDE_LENGTH)];
    CBox texbox = CBox{OUTER + (openedID % SIDE_LENGTH) * tileRenderSize.x + (openedID % SIDE_LENGTH) * GAPSIZE,
                       OUTER + (openedID / SIDE_LENGTH) * tileRenderSize.y + (openedID / SIDE_LENGTH) * GAPSIZE, tileRenderSize.x, tileRenderSize.y}
                      .translate(pMonitor->m_position);

    damage();

    blockDamageReporting = true;
    g_pHyprRenderer->damageBox(texbox);
    blockDamageReporting = false;
    g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void COverview::close() {
    if (closing)
        return;

    resetSubmapIfNeeded();

    const int   ID = closeOnID == -1 ? openedID : closeOnID;

    const auto& TILE = images[std::clamp(ID, 0, SIDE_LENGTH * SIDE_LENGTH)];

    Vector2D    tileSize = (pMonitor->m_size / SIDE_LENGTH);

    *size = pMonitor->m_size * pMonitor->m_size / tileSize;
    *pos  = (-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{ID % SIDE_LENGTH, ID / SIDE_LENGTH}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

    size->setCallbackOnEnd(removeOverview);

    closing = true;

    redrawAll();

    if (TILE.workspaceID != pMonitor->activeWorkspaceID()) {
        pMonitor->setSpecialWorkspace(0);

        // If this tile's workspace was WORKSPACE_INVALID, move to the next
        // empty workspace. This should only happen if skip_empty is on, in
        // which case some tiles will be left with this ID intentionally.
        const int  NEWID = TILE.workspaceID == WORKSPACE_INVALID ? getWorkspaceIDNameFromString("emptynm").id : TILE.workspaceID;

        const auto NEWIDWS = g_pCompositor->getWorkspaceByID(NEWID);

        const auto OLDWS = pMonitor->m_activeWorkspace;

        if (!NEWIDWS)
            g_pKeybindManager->changeworkspace(std::to_string(NEWID));
        else
            g_pKeybindManager->changeworkspace(NEWIDWS->getConfigName());

        g_pDesktopAnimationManager->startAnimation(pMonitor->m_activeWorkspace, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);
        g_pDesktopAnimationManager->startAnimation(OLDWS, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);

        startedOn = pMonitor->m_activeWorkspace;
    }
}

void COverview::onPreRender() {
    if (damageDirty) {
        damageDirty = false;
        redrawID(closing ? (closeOnID == -1 ? openedID : closeOnID) : openedID);
    }
}

void COverview::onWorkspaceChange() {
    if (valid(startedOn))
        g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_OUT, false, true);
    else
        startedOn = pMonitor->m_activeWorkspace;

    for (size_t i = 0; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
        if (images[i].workspaceID != pMonitor->activeWorkspaceID())
            continue;

        openedID = i;
        break;
    }

    closeOnID = openedID;
    close();
}

void COverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>());
}

void COverview::fullRender() {
    const auto GAPSIZE = (closing ? (1.0 - size->getPercent()) : size->getPercent()) * GAP_WIDTH;

    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    Vector2D SIZE = size->value();

    static auto* const* PGAPSO = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gaps_out")->getDataStaticPtr();
    const float OUTER = **PGAPSO * (closing ? (1.0 - size->getPercent()) : size->getPercent());

    Vector2D tileSize       = (SIZE / SIDE_LENGTH);
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (SIDE_LENGTH - 1) - Vector2D{OUTER * 2, OUTER * 2}) / SIDE_LENGTH;

    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    static auto* const* PTILEROUND = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding")->getDataStaticPtr();
    static auto* const* PTOUNDPWR  = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_power")->getDataStaticPtr();
    static auto* const* PTILEROUNDF = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_focus")->getDataStaticPtr();
    static auto* const* PTILEROUNDC = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_current")->getDataStaticPtr();
    static auto* const* PTILEROUNDH = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_hover")->getDataStaticPtr();

    const int BASE_ROUND_SCALED   = std::max(0, (int)std::lround((double)**PTILEROUND * pMonitor->m_scale));
    const int FOCUS_ROUND_SCALED  = **PTILEROUNDF >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDF * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const int CURRENT_ROUND_SCALED= **PTILEROUNDC >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDC * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const int HOVER_ROUND_SCALED  = **PTILEROUNDH >= 0 ? std::max(0, (int)std::lround((double)**PTILEROUNDH * pMonitor->m_scale)) : BASE_ROUND_SCALED;
    const float ROUND_PWR         = **PTOUNDPWR;

    // (shadows moved to feature/shadows branch)

    for (size_t y = 0; y < (size_t)SIDE_LENGTH; ++y) {
        for (size_t x = 0; x < (size_t)SIDE_LENGTH; ++x) {
            const int id = x + y * SIDE_LENGTH;
            CBox      texbox{OUTER + x * tileRenderSize.x + x * GAPSIZE, OUTER + y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
            texbox.scale(pMonitor->m_scale).translate(pos->value());
            texbox.round();
            // per-tile rounding override for focus/current/hover (priority: focus > current > hover)
            int tileRound = BASE_ROUND_SCALED;
            if ((int)id == kbFocusID)
                tileRound = FOCUS_ROUND_SCALED;
            else if ((int)id == openedID)
                tileRound = CURRENT_ROUND_SCALED;
            else if ((int)id == hoveredID)
                tileRound = HOVER_ROUND_SCALED;

            // clamp rounding to tile size
            const int maxCornerPx = std::max(0, (int)std::floor(std::min(texbox.w, texbox.h) / 2.0));
            tileRound = std::min(tileRound, maxCornerPx);

            // no shadow in this branch

            CRegion damage{0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderTextureInternal(images[id].fb.getTexture(), texbox, {.damage = &damage, .a = 1.0, .round = tileRound, .roundingPower = ROUND_PWR});
        }
    }

    // overlays: numbers and borders
    static auto* const* PLABELEN   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_enable")->getDataStaticPtr();
    static auto* const* PLABELCOL  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color")->getDataStaticPtr();
    static auto* const* PLABELSIZE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_font_size")->getDataStaticPtr();
    static auto  const* PLABELPOS  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_position")->getDataStaticPtr();
    static auto  const* PLABELMODE = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_text_mode")->getDataStaticPtr();
    static auto  const* PTOKENMAP  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_token_map")->getDataStaticPtr();
    static auto* const* PLABELOX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_x")->getDataStaticPtr();
    static auto* const* PLABELOY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_y")->getDataStaticPtr();
    static auto  const* PLABELSHOW = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_show")->getDataStaticPtr();
    static auto* const* PLCOLDEF   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_default")->getDataStaticPtr();
    static auto* const* PLCOLHOV   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_hover")->getDataStaticPtr();
    static auto* const* PLCOLFOC   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_focus")->getDataStaticPtr();
    static auto* const* PLCOLCUR   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_color_current")->getDataStaticPtr();
    static auto* const* PLSCALEH   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_hover")->getDataStaticPtr();
    static auto* const* PLSCALEF   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_focus")->getDataStaticPtr();
    static auto* const* PLBGEN     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_enable")->getDataStaticPtr();
    static auto* const* PLBGCOL    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_color")->getDataStaticPtr();
    static auto* const* PLBGROUND  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_rounding")->getDataStaticPtr();
    static auto  const* PLBGSHAPE  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_shape")->getDataStaticPtr();
    static auto* const* PLBGPAD    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_padding")->getDataStaticPtr();

    static auto* const* PBWIDTH      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_width")->getDataStaticPtr();
    static auto  const* PBCOLCUR     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_current")->getDataStaticPtr();
    static auto  const* PBCOLFOC     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_focus")->getDataStaticPtr();
    static auto  const* PBCOLHOV     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_color_hover")->getDataStaticPtr();
    // Deprecated configs for backwards compatibility
    static auto  const* PBGRCUR      = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_current")->getDataStaticPtr();
    static auto  const* PBGREFOC     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_focus")->getDataStaticPtr();
    static auto  const* PBGREHOV     = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_hover")->getDataStaticPtr();

    // draw labels
    if (**PLABELEN) {
        // use the tracked hoveredID (cleared during closing)
        const int labelHoveredID = closing ? -1 : hoveredID;

        auto shouldShow = [&](int id) -> bool {
            if (std::string{*PLABELSHOW} == "never")
                return false;
            if (std::string{*PLABELSHOW} == "always")
                return true;
            const bool isHover  = id == labelHoveredID;
            const bool isFocus  = id == kbFocusID;
            const bool isCurr   = id == openedID;
            const std::string mode{*PLABELSHOW};
            if (mode == "hover")
                return isHover;
            if (mode == "focus")
                return isFocus;
            if (mode == "hover+focus")
                return isHover || isFocus;
            if (mode == "current+focus")
                return isCurr || isFocus;
            return true;
        };

        auto resolveState = [&](int id) -> int {
            // precedence: focus > current > hover > default
            if (id == kbFocusID)
                return 2; // focus
            if (id == openedID)
                return 3; // current
            if (id == labelHoveredID)
                return 1; // hover
            return 0;     // default
        };

        auto placeBox = [&](const CBox& tile, const Vector2D& size) -> CBox {
            double x = tile.x, y = tile.y;
            const std::string pos{*PLABELPOS};
            if (pos == "top-left") {
                x += **PLABELOX; y += **PLABELOY;
            } else if (pos == "top-right") {
                x += tile.w - size.x - **PLABELOX; y += **PLABELOY;
            } else if (pos == "bottom-left") {
                x += **PLABELOX; y += tile.h - size.y - **PLABELOY;
            } else if (pos == "bottom-right") {
                x += tile.w - size.x - **PLABELOX; y += tile.h - size.y - **PLABELOY;
            } else { // center
                x += (tile.w - size.x) / 2.0; y += (tile.h - size.y) / 2.0;
            }
            return CBox{x, y, (double)size.x, (double)size.y};
        };

        int tokenCounter = 0;
        for (size_t y = 0; y < (size_t)SIDE_LENGTH; ++y) {
            for (size_t x = 0; x < (size_t)SIDE_LENGTH; ++x) {
                const int id = x + y * SIDE_LENGTH;
                if (images[id].workspaceID == WORKSPACE_INVALID)
                    continue;

                // compute tile box again for label placement
                CBox tile{OUTER + x * tileRenderSize.x + x * GAPSIZE, OUTER + y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
                tile.scale(pMonitor->m_scale).translate(pos->value());
                tile.round();

                std::string label;
                const std::string mode{*PLABELMODE};
                if (mode == "token") {
                    // override map (comma-separated) up to 50
                    std::vector<std::string> tokens;
                    if (!std::string{*PTOKENMAP}.empty()) {
                        const std::string mapStr{*PTOKENMAP};
                        size_t            start = 0;
                        for (size_t cur = 0; cur <= mapStr.size(); ++cur) {
                            if (cur == mapStr.size() || mapStr[cur] == ',') {
                                std::string t = mapStr.substr(start, cur - start);
                                while (!t.empty() && std::isspace((unsigned char)t.front())) t.erase(t.begin());
                                while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
                                tokens.push_back(t);
                                start = cur + 1;
                            }
                        }
                    }

                    const int k = tokenCounter; // 0-based visible index
                    if (k < (int)tokens.size() && !tokens[k].empty())
                        label = tokens[k];
                    else {
                        if (k <= 8) label = std::to_string(k + 1);       // 1..9
                        else if (k == 9) label = "0";                   // 10 -> 0
                        else if (k >= 10 && k <= 19) {                   // 11..20
                            static const char* SYM[10] = {"!","@","#","$","%","^","&","*","(",")"};
                            label = SYM[k - 10];
                        } else if (k >= 20 && k <= 45) {
                            label = std::string(1, char('a' + (k - 20))); // 21..46 -> a..z
                        } else {
                            label = ""; // 47..50 blank by default
                        }
                    }
                } else if (mode == "index") {
                    label = std::to_string(tokenCounter + 1);
                } else {
                    label = std::to_string(images[id].workspaceID);
                }
                const int         baseF = std::max(8, (int)**PLABELSIZE);
                const int         st    = resolveState(id);

                auto ensureTex = [&](SP<CTexture>& tex, Vector2D& sz, const CHyprColor& col, float scaleMul) {
                    if (!tex)
                        tex = makeShared<CTexture>();
                    if (tex->m_texID == 0) {
                        const int fsz = std::max(8, (int)std::round(baseF * scaleMul));
                        Vector2D  buf{std::max(32, fsz * 2), std::max(24, fsz + 8)};
                        sz = buf;
                        renderNumberTexture(tex, label, col, buf, pMonitor->m_scale, fsz);
                    }
                };

                // if label isn't shown per mode or label is empty, still advance token index
                if (!shouldShow(id) || label.empty()) { tokenCounter++; continue; }

                CHyprColor col = CHyprColor{(uint64_t)**PLCOLDEF};
                float      scl = 1.0f;
                static auto* const* PLPIXELSNAP = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:label_pixel_snap")->getDataStaticPtr();

                auto drawWithBG = [&](SP<CTexture>& tex, const Vector2D& tsize) {
                    const int pad = **PLBGPAD;
                    // background size
                    Vector2D bgSize = {tsize.x + pad * 2, tsize.y + pad * 2};
                    const std::string shape{*PLBGSHAPE};
                    int roundPx = **PLBGROUND;
                    if (shape == "circle" || shape == "square") {
                        const double side = std::max(bgSize.x, bgSize.y);
                        bgSize            = {side, side};
                        roundPx           = (shape == "circle") ? std::lround(side / 2.0) : 0;
                    }
                    CBox bg = placeBox(tile, bgSize);
                    // center text within bg
                    CBox lb{bg.x + (bg.w - tsize.x) / 2.0, bg.y + (bg.h - tsize.y) / 2.0, (double)tsize.x, (double)tsize.y};
                    if (**PLPIXELSNAP) {
                        bg.round();
                        lb.round();
                    }
                    // draw
                    g_pHyprOpenGL->renderRect(bg, CHyprColor{(uint64_t)**PLBGCOL}, {.round = roundPx});
                    g_pHyprOpenGL->renderTexture(tex, lb, {.a = 1.0});
                };

                auto drawNoBG = [&](SP<CTexture>& tex, const Vector2D& tsize) {
                    CBox lb = placeBox(tile, tsize);
                    if (**PLPIXELSNAP)
                        lb.round();
                    g_pHyprOpenGL->renderTexture(tex, lb, {.a = 1.0});
                };

                if (st == 1) { // hover
                    col = CHyprColor{(uint64_t)**PLCOLHOV};
                    scl = **PLSCALEH;
                    ensureTex(images[id].labelTexHover, images[id].labelSizeHover, col, scl);
                    if (**PLBGEN)
                        drawWithBG(images[id].labelTexHover, images[id].labelSizeHover);
                    else
                        drawNoBG(images[id].labelTexHover, images[id].labelSizeHover);
                } else if (st == 2) { // focus
                    col = CHyprColor{(uint64_t)**PLCOLFOC};
                    scl = **PLSCALEF;
                    ensureTex(images[id].labelTexFocus, images[id].labelSizeFocus, col, scl);
                    if (**PLBGEN)
                        drawWithBG(images[id].labelTexFocus, images[id].labelSizeFocus);
                    else
                        drawNoBG(images[id].labelTexFocus, images[id].labelSizeFocus);
                } else if (st == 3) { // current
                    col = CHyprColor{(uint64_t)**PLCOLCUR};
                    ensureTex(images[id].labelTexCurrent, images[id].labelSizeCurrent, col, 1.0f);
                    if (**PLBGEN)
                        drawWithBG(images[id].labelTexCurrent, images[id].labelSizeCurrent);
                    else
                        drawNoBG(images[id].labelTexCurrent, images[id].labelSizeCurrent);
                } else { // default
                    ensureTex(images[id].labelTexDefault, images[id].labelSizeDefault, CHyprColor{(uint64_t)**PLCOLDEF}, 1.0f);
                    if (**PLBGEN)
                        drawWithBG(images[id].labelTexDefault, images[id].labelSizeDefault);
                    else
                        drawNoBG(images[id].labelTexDefault, images[id].labelSizeDefault);
                }
                tokenCounter++;
            }
        }
    }

    // draw borders for hover, current and focus (priority order: focus > current > hover)

    // pass rounding based on state
    const int RND_CUR = CURRENT_ROUND_SCALED;
    const int RND_FOC = FOCUS_ROUND_SCALED;
    const int RND_HOV = HOVER_ROUND_SCALED;

    // Helper to parse border config (supports rgb/hex/gradient, with deprecated fallback)
    auto drawBorderForID = [&](int id, const std::string& borderSpec, const std::string& deprecatedGradSpec, int roundScaled) {
        if (id < 0)
            return;
        const int ix = id % SIDE_LENGTH;
        const int iy = id / SIDE_LENGTH;
        CBox       box{OUTER + ix * tileRenderSize.x + ix * GAPSIZE, OUTER + iy * tileRenderSize.y + iy * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
        box.scale(pMonitor->m_scale).translate(pos->value());
        box.round();
        const int BWIDTH = std::max(1, (int)**PBWIDTH);

        // Determine which spec to use (prefer new format, fallback to deprecated)
        std::string effectiveSpec = borderSpec.empty() ? deprecatedGradSpec : borderSpec;

        // Auto-detect format: gradient vs solid color
        if (isGradientBorderSpec(effectiveSpec)) {
            // Render as gradient border (hyprland style)
            const auto spec = parseGradientSpec(effectiveSpec);
            if (spec.valid) {
                CGradientValueData grad;
                grad.m_colors.clear();
                grad.m_colors.push_back(spec.c1);
                grad.m_colors.push_back(spec.c2);
                grad.m_angle = spec.angleDeg * (float)M_PI / 180.f;
                grad.updateColorsOk();
                g_pHyprOpenGL->renderBorder(box, grad, {.round = roundScaled, .roundingPower = ROUND_PWR, .borderSize = BWIDTH});
            }
        } else if (!effectiveSpec.empty()) {
            // Parse as solid color (rgb/hex format)
            CHyprColor color;

            // Handle rgb() format
            if (effectiveSpec.find("rgb(") == 0) {
                // Extract hex from rgb(rrggbb)
                size_t start = effectiveSpec.find('(');
                size_t end = effectiveSpec.find(')');
                if (start != std::string::npos && end != std::string::npos) {
                    std::string hexStr = effectiveSpec.substr(start + 1, end - start - 1);
                    // Parse as hex, prepend FF for alpha
                    color = CHyprColor{(uint64_t)std::stoull("FF" + hexStr, nullptr, 16)};
                }
            } else {
                // Handle 0xAARRGGBB format directly
                color = CHyprColor{(uint64_t)std::stoull(effectiveSpec, nullptr, 16)};
            }

            // Render as simple border
            g_pHyprOpenGL->renderBorder(box, color, {.round = roundScaled, .roundingPower = ROUND_PWR, .borderSize = BWIDTH});
        }
    };

    // Draw borders in order: hover (lowest), then current, then focus (highest priority)
    if (hoveredID != -1 && hoveredID != openedID && hoveredID != kbFocusID)
        drawBorderForID(hoveredID, std::string{*PBCOLHOV}, std::string{*PBGREHOV}, RND_HOV);
    drawBorderForID(openedID, std::string{*PBCOLCUR}, std::string{*PBGRCUR}, RND_CUR);
    if (kbFocusID != -1)
        drawBorderForID(kbFocusID, std::string{*PBCOLFOC}, std::string{*PBGREFOC}, RND_FOC);
}

static float lerp(const float& from, const float& to, const float perc) {
    return (to - from) * perc + from;
}

static Vector2D lerp(const Vector2D& from, const Vector2D& to, const float perc) {
    return Vector2D{lerp(from.x, to.x, perc), lerp(from.y, to.y, perc)};
}

void COverview::setClosing(bool closing_) {
    closing = closing_;
}

void COverview::resetSwipe() {
    swipeWasCommenced = false;
}

void COverview::onSwipeUpdate(double delta) {
    m_isSwiping = true;

    if (swipeWasCommenced)
        return;

    static auto* const* PDISTANCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance")->getDataStaticPtr();

    const float         PERC               = closing ? std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0) : 1.0 - std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0);
    const auto          WORKSPACE_FOCUS_ID = closing && closeOnID != -1 ? closeOnID : openedID;

    Vector2D            tileSize = (pMonitor->m_size / SIDE_LENGTH);

    const auto          SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
    const auto          POSMAX  = (-((pMonitor->m_size / (double)SIDE_LENGTH) * Vector2D{WORKSPACE_FOCUS_ID % SIDE_LENGTH, WORKSPACE_FOCUS_ID / SIDE_LENGTH}) * pMonitor->m_scale) *
        (pMonitor->m_size / tileSize);

    const auto SIZEMIN = pMonitor->m_size;
    const auto POSMIN  = Vector2D{0, 0};

    size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
    pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void COverview::onSwipeEnd() {
    const auto SIZEMIN = pMonitor->m_size;
    const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / (pMonitor->m_size / SIDE_LENGTH);
    const auto PERC    = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
    if (PERC > 0.5) {
        close();
        return;
    }
    *size = pMonitor->m_size;
    *pos  = {0, 0};

    size->setCallbackOnEnd([this](WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) { redrawAll(true); });

    swipeWasCommenced = true;
    m_isSwiping       = false;
}

void COverview::enterSubmapIfEnabled() {
    static auto* const* PKEYNAV = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:keynav_enable")->getDataStaticPtr();
    if (**PKEYNAV && !submapActive) {
        // switch to a dedicated submap for hyprexpo navigation
        g_pKeybindManager->m_dispatchers["submap"]("hyprexpo");
        submapActive = true;
    }
}

void COverview::resetSubmapIfNeeded() {
    if (submapActive) {
        g_pKeybindManager->m_dispatchers["submap"]("reset");
        submapActive = false;
    }
}
