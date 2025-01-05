#include "overview.hpp"
#include <any>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#undef private
#include "OverviewPassElement.hpp"

static void damageMonitor(void*) {
    g_pOverview->damage();
}

static void removeOverview(void*) {
    g_pOverview.reset();
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pInputManager->unsetCursorImage();
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

COverview::COverview(PHLWORKSPACE startedOn_, bool swipe_) : startedOn(startedOn_), swipe(swipe_) {
    const auto PMONITOR = g_pCompositor->m_pLastMonitor.lock();
    pMonitor            = PMONITOR;

    static auto* const* PCOLUMNS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:columns")->getDataStaticPtr();
    static auto* const* PGAPS    = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gap_size")->getDataStaticPtr();
    static auto* const* PCOL     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:bg_col")->getDataStaticPtr();
    static auto const*  PMETHOD  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method")->getDataStaticPtr();

    SIDE_LENGTH = **PCOLUMNS;
    GAP_WIDTH   = **PGAPS;
    BG_COLOR    = **PCOL;

    // process the method
    bool     methodCenter  = true;
    int      methodStartID = pMonitor->activeWorkspaceID();
    CVarList method{*PMETHOD, 0, 's', true};
    if (method.size() < 2)
        Debug::log(ERR, "[he] invalid workspace_method");
    else {
        methodCenter  = method[0] == "center";
        methodStartID = getWorkspaceIDNameFromString(method[1]).id;
        if (methodStartID == WORKSPACE_INVALID)
            methodStartID = pMonitor->activeWorkspaceID();
    }

    images.resize(SIDE_LENGTH * SIDE_LENGTH);

    if (methodCenter) {
        int currentID = methodStartID;
        int firstID   = currentID;

        int backtracked = 0;

        for (size_t i = 1; i < images.size() / 2; ++i) {
            currentID = getWorkspaceIDNameFromString("r-" + std::to_string(i)).id;
            if (currentID >= firstID)
                break;

            backtracked++;
            firstID = currentID;
        }

        for (size_t i = 0; i < SIDE_LENGTH * SIDE_LENGTH; ++i) {
            auto& image = images[i];
            currentID =
                getWorkspaceIDNameFromString("r" + ((int64_t)i - backtracked < 0 ? std::to_string((int64_t)i - backtracked) : "+" + std::to_string((int64_t)i - backtracked))).id;
            image.workspaceID = currentID;
        }
    } else {
        int currentID         = methodStartID;
        images[0].workspaceID = currentID;

        auto PWORKSPACESTART = g_pCompositor->getWorkspaceByID(currentID);
        if (!PWORKSPACESTART)
            PWORKSPACESTART = CWorkspace::create(currentID, pMonitor.lock(), std::to_string(currentID));

        pMonitor->activeWorkspace = PWORKSPACESTART;

        for (size_t i = 1; i < SIDE_LENGTH * SIDE_LENGTH; ++i) {
            auto& image       = images[i];
            currentID         = getWorkspaceIDNameFromString("r+" + std::to_string(i)).id;
            image.workspaceID = currentID;
        }

        pMonitor->activeWorkspace = startedOn;
    }

    g_pHyprRenderer->makeEGLCurrent();

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    Vector2D tileSize       = pMonitor->vecSize / SIDE_LENGTH;
    Vector2D tileRenderSize = (pMonitor->vecSize - Vector2D{GAP_WIDTH * pMonitor->scale, GAP_WIDTH * pMonitor->scale} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->vecPixelSize};

    int          currentid = 0;

    PHLWORKSPACE openSpecial = PMONITOR->activeSpecialWorkspace;
    if (openSpecial)
        PMONITOR->activeSpecialWorkspace.reset();

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

    startedOn->m_bVisible = false;

    for (size_t i = 0; i < SIDE_LENGTH * SIDE_LENGTH; ++i) {
        COverview::SWorkspaceImage& image = images[i];
        image.fb.alloc(monbox.w, monbox.h, PMONITOR->output->state->state().drmFormat);

        CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

        g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(image.workspaceID);

        if (PWORKSPACE == startedOn)
            currentid = i;

        if (PWORKSPACE) {
            image.pWorkspace          = PWORKSPACE;
            PMONITOR->activeWorkspace = PWORKSPACE;
            PWORKSPACE->startAnim(true, true, true);
            PWORKSPACE->m_bVisible = true;

            if (PWORKSPACE == startedOn)
                PMONITOR->activeSpecialWorkspace = openSpecial;

            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, &now, monbox);

            PWORKSPACE->m_bVisible = false;
            PWORKSPACE->startAnim(false, false, true);

            if (PWORKSPACE == startedOn)
                PMONITOR->activeSpecialWorkspace.reset();
        } else
            g_pHyprRenderer->renderWorkspace(PMONITOR, PWORKSPACE, &now, monbox);

        image.box = {(i % SIDE_LENGTH) * tileRenderSize.x + (i % SIDE_LENGTH) * GAP_WIDTH, (i / SIDE_LENGTH) * tileRenderSize.y + (i / SIDE_LENGTH) * GAP_WIDTH, tileRenderSize.x,
                     tileRenderSize.y};

        g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    PMONITOR->activeSpecialWorkspace = openSpecial;
    PMONITOR->activeWorkspace        = startedOn;
    startedOn->m_bVisible            = true;
    startedOn->startAnim(true, true, true);

    // zoom on the current workspace.
    const auto& TILE = images[std::clamp(currentid, 0, SIDE_LENGTH * SIDE_LENGTH)];

    size.create(pMonitor->vecSize * pMonitor->vecSize / tileSize, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    pos.create((-((pMonitor->vecSize / (double)SIDE_LENGTH) * Vector2D{currentid % SIDE_LENGTH, currentid / SIDE_LENGTH}) * pMonitor->scale) * (pMonitor->vecSize / tileSize),
               g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    size.setUpdateCallback(damageMonitor);
    pos.setUpdateCallback(damageMonitor);

    if (!swipe) {
        size = pMonitor->vecSize;
        pos  = {0, 0};

        size.setCallbackOnEnd([this](void*) { redrawAll(true); });
    }

    openedID = currentid;

    g_pInputManager->setCursorImageUntilUnset("left_ptr");

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->vecPosition;

    auto onCursorMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->vecPosition;
    };

    auto onCursorSelect = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        // get tile x,y
        int x = lastMousePosLocal.x / pMonitor->vecSize.x * SIDE_LENGTH;
        int y = lastMousePosLocal.y / pMonitor->vecSize.y * SIDE_LENGTH;

        closeOnID = x + y * SIDE_LENGTH;

        close();
    };

    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onCursorMove);
    touchMoveHook = g_pHookSystem->hookDynamic("touchMove", onCursorMove);

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onCursorSelect);
    touchDownHook   = g_pHookSystem->hookDynamic("touchDown", onCursorSelect);
}

void COverview::redrawID(int id, bool forcelowres) {
    if (pMonitor->activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    id = std::clamp(id, 0, SIDE_LENGTH * SIDE_LENGTH);

    Vector2D tileSize       = pMonitor->vecSize / SIDE_LENGTH;
    Vector2D tileRenderSize = (pMonitor->vecSize - Vector2D{GAP_WIDTH, GAP_WIDTH} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!forcelowres && (size.value() != pMonitor->vecSize || closing))
        monbox = {{0, 0}, pMonitor->vecPixelSize};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->vecPixelSize};

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    auto& image = images[id];

    if (image.fb.m_vSize != monbox.size()) {
        image.fb.release();
        image.fb.alloc(monbox.w, monbox.h, pMonitor->output->state->state().drmFormat);
    }

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &image.fb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    const auto   PWORKSPACE = image.pWorkspace;

    PHLWORKSPACE openSpecial = pMonitor->activeSpecialWorkspace;
    if (openSpecial)
        pMonitor->activeSpecialWorkspace.reset();

    startedOn->m_bVisible = false;

    if (PWORKSPACE) {
        pMonitor->activeWorkspace = PWORKSPACE;
        PWORKSPACE->startAnim(true, true, true);
        PWORKSPACE->m_bVisible = true;

        if (PWORKSPACE == startedOn)
            pMonitor->activeSpecialWorkspace = openSpecial;

        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, &now, monbox);

        PWORKSPACE->m_bVisible = false;
        PWORKSPACE->startAnim(false, false, true);

        if (PWORKSPACE == startedOn)
            pMonitor->activeSpecialWorkspace.reset();
    } else
        g_pHyprRenderer->renderWorkspace(pMonitor.lock(), PWORKSPACE, &now, monbox);

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    pMonitor->activeSpecialWorkspace = openSpecial;
    pMonitor->activeWorkspace        = startedOn;
    startedOn->m_bVisible            = true;
    startedOn->startAnim(true, true, true);

    blockOverviewRendering = false;
}

void COverview::redrawAll(bool forcelowres) {
    for (size_t i = 0; i < SIDE_LENGTH * SIDE_LENGTH; ++i) {
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

    Vector2D    SIZE = size.value();

    Vector2D    tileSize       = (SIZE / SIDE_LENGTH);
    Vector2D    tileRenderSize = (SIZE - Vector2D{GAP_WIDTH, GAP_WIDTH} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;
    const auto& TILE           = images[std::clamp(openedID, 0, SIDE_LENGTH * SIDE_LENGTH)];
    CBox        texbox         = CBox{(openedID % SIDE_LENGTH) * tileRenderSize.x + (openedID % SIDE_LENGTH) * GAP_WIDTH,
                       (openedID / SIDE_LENGTH) * tileRenderSize.y + (openedID / SIDE_LENGTH) * GAP_WIDTH, tileRenderSize.x, tileRenderSize.y}
                      .translate(pMonitor->vecPosition);

    damage();

    blockDamageReporting = true;
    g_pHyprRenderer->damageBox(&texbox);
    blockDamageReporting = false;
    g_pCompositor->scheduleFrameForMonitor(pMonitor.lock());
}

void COverview::close() {
    if (closing)
        return;

    const int   ID = closeOnID == -1 ? openedID : closeOnID;

    const auto& TILE = images[std::clamp(ID, 0, SIDE_LENGTH * SIDE_LENGTH)];

    Vector2D    tileSize = (pMonitor->vecSize / SIDE_LENGTH);

    size = pMonitor->vecSize * pMonitor->vecSize / tileSize;
    pos  = (-((pMonitor->vecSize / (double)SIDE_LENGTH) * Vector2D{ID % SIDE_LENGTH, ID / SIDE_LENGTH}) * pMonitor->scale) * (pMonitor->vecSize / tileSize);

    size.setCallbackOnEnd(removeOverview);

    closing = true;

    redrawAll();

    if (TILE.workspaceID != pMonitor->activeWorkspaceID()) {
        pMonitor->setSpecialWorkspace(0);

        const auto NEWIDWS = g_pCompositor->getWorkspaceByID(TILE.workspaceID);

        const auto OLDWS = pMonitor->activeWorkspace;

        if (!NEWIDWS)
            g_pKeybindManager->changeworkspace(std::to_string(TILE.workspaceID));
        else
            g_pKeybindManager->changeworkspace(NEWIDWS->getConfigName());

        pMonitor->activeWorkspace->startAnim(true, true, true);
        OLDWS->startAnim(false, false, true);

        startedOn = pMonitor->activeWorkspace;
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
        startedOn->startAnim(false, false, true);
    else
        startedOn = pMonitor->activeWorkspace;

    for (size_t i = 0; i < SIDE_LENGTH * SIDE_LENGTH; ++i) {
        if (images[i].workspaceID != pMonitor->activeWorkspaceID())
            continue;

        openedID = i;
        break;
    }

    closeOnID = openedID;
    close();
}

void COverview::render() {
    g_pHyprRenderer->m_sRenderPass.add(makeShared<COverviewPassElement>());
}

void COverview::fullRender() {
    const auto GAPSIZE = (closing ? (1.0 - size.getPercent()) : size.getPercent()) * GAP_WIDTH;

    if (pMonitor->activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    Vector2D SIZE = size.value();

    Vector2D tileSize       = (SIZE / SIDE_LENGTH);
    Vector2D tileRenderSize = (SIZE - Vector2D{GAPSIZE, GAPSIZE} * (SIDE_LENGTH - 1)) / SIDE_LENGTH;

    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    for (size_t y = 0; y < SIDE_LENGTH; ++y) {
        for (size_t x = 0; x < SIDE_LENGTH; ++x) {
            CBox texbox = {x * tileRenderSize.x + x * GAPSIZE, y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
            texbox.scale(pMonitor->scale).translate(pos.value());
            texbox.round();
            CRegion damage{0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderTextureInternalWithDamage(images[x + y * SIDE_LENGTH].fb.getTexture(), &texbox, 1.0, damage);
        }
    }
}

static float lerp(const float& from, const float& to, const float perc) {
    return (to - from) * perc + from;
}

static Vector2D lerp(const Vector2D& from, const Vector2D& to, const float perc) {
    return Vector2D{lerp(from.x, to.x, perc), lerp(from.y, to.y, perc)};
}

void COverview::onSwipeUpdate(double delta) {
    if (swipeWasCommenced)
        return;

    static auto* const* PDISTANCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance")->getDataStaticPtr();

    const float         PERC = 1.0 - std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0);

    Vector2D            tileSize = (pMonitor->vecSize / SIDE_LENGTH);

    const auto          SIZEMAX = pMonitor->vecSize * pMonitor->vecSize / tileSize;
    const auto          POSMAX =
        (-((pMonitor->vecSize / (double)SIDE_LENGTH) * Vector2D{openedID % SIDE_LENGTH, openedID / SIDE_LENGTH}) * pMonitor->scale) * (pMonitor->vecSize / tileSize);

    const auto SIZEMIN = pMonitor->vecSize;
    const auto POSMIN  = Vector2D{0, 0};

    size.setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
    pos.setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void COverview::onSwipeEnd() {
    const auto SIZEMIN = pMonitor->vecSize;
    const auto SIZEMAX = pMonitor->vecSize * pMonitor->vecSize / (pMonitor->vecSize / SIDE_LENGTH);
    const auto PERC    = (size.value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
    if (PERC > 0.5) {
        close();
        return;
    }
    size = pMonitor->vecSize;
    pos  = {0, 0};

    size.setCallbackOnEnd([this](void*) { redrawAll(true); });

    swipeWasCommenced = true;
}
