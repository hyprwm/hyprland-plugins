#include "overview.hpp"
#include <any>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/cursor/CursorShapeOverrideController.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"

// Given a cell size, return a box (with offsets) that preserves monAspect within the cell
static CBox aspectCorrectBox(double cellW, double cellH, double monAspect) {
    double cellAspect = cellW / cellH;
    double tileW, tileH;
    if (cellAspect > monAspect) {
        tileH = cellH;
        tileW = cellH * monAspect;
    } else {
        tileW = cellW;
        tileH = cellW / monAspect;
    }
    double padX = (cellW - tileW) / 2.0;
    double padY = (cellH - tileH) / 2.0;
    return {padX, padY, tileW, tileH};
}

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

COverview::~COverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    Cursor::overrideController->unsetOverride(Cursor::CURSOR_OVERRIDE_UNKNOWN);
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

COverview::COverview(PHLWORKSPACE startedOn_, bool swipe_) : startedOn(startedOn_), swipe(swipe_) {
    const auto PMONITOR = Desktop::focusState()->monitor();
    pMonitor            = PMONITOR;

    static auto* const* PCOLUMNS  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:columns")->getDataStaticPtr();
    static auto* const* PGAPS     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gap_size")->getDataStaticPtr();
    static auto* const* PCOL      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:bg_col")->getDataStaticPtr();
    static auto* const* PSKIP     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:skip_empty")->getDataStaticPtr();
    static auto* const* PDYNAMIC  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:dynamic_grid")->getDataStaticPtr();
    static auto const*  PMETHOD   = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method")->getDataStaticPtr();

    // Dynamic grid: count active workspaces and calculate optimal grid size
    if (**PDYNAMIC) {
        dynamicGrid = true;
        int activeCount = 0;
        for (auto& ws : g_pCompositor->m_workspaces) {
            if (!ws->m_isSpecialWorkspace && ws->getWindows() > 0)
                activeCount++;
        }
        activeCount = std::max(activeCount, 1);
        SIDE_LENGTH = (int)std::ceil(std::sqrt((double)activeCount));  // columns
        gridRows = (int)std::ceil((double)activeCount / SIDE_LENGTH);   // rows
        // Ensure minimum 2x2 grid for better aspect ratio (unless only 1 workspace)
        if (activeCount > 1 && gridRows < 2) {
            gridRows = 2;
        }
    } else {
        dynamicGrid = false;
        SIDE_LENGTH = **PCOLUMNS;
        gridRows = SIDE_LENGTH;
    }
    GAP_WIDTH   = **PGAPS;
    BG_COLOR    = **PCOL;

    // process the method
    bool     methodCenter  = true;
    int      methodStartID = pMonitor->activeWorkspaceID();
    CVarList method{*PMETHOD, 0, 's', true};
    if (method.size() < 2)
        Log::logger->log(Log::ERR, "[he] invalid workspace_method");
    else {
        methodCenter  = method[0] == "center";
        methodStartID = getWorkspaceIDNameFromString(method[1]).id;
        if (methodStartID == WORKSPACE_INVALID)
            methodStartID = pMonitor->activeWorkspaceID();
    }

    // For dynamic grid, collect active workspace IDs; otherwise use standard grid
    if (dynamicGrid) {
        std::vector<int64_t> activeWorkspaceIDs;
        for (auto& ws : g_pCompositor->m_workspaces) {
            if (!ws->m_isSpecialWorkspace && ws->getWindows() > 0)
                activeWorkspaceIDs.push_back(ws->m_id);
        }
        std::sort(activeWorkspaceIDs.begin(), activeWorkspaceIDs.end());
        if (activeWorkspaceIDs.empty())
            activeWorkspaceIDs.push_back(pMonitor->activeWorkspaceID());

        images.resize(activeWorkspaceIDs.size());
        for (size_t i = 0; i < activeWorkspaceIDs.size(); ++i)
            images[i].workspaceID = activeWorkspaceIDs[i];
    } else {
        images.resize(SIDE_LENGTH * SIDE_LENGTH);

        // r includes empty workspaces; m skips over them
        std::string selector = **PSKIP ? "m" : "r";

        if (methodCenter) {
            int currentID = methodStartID;
            int firstID   = currentID;

            int backtracked = 0;

            // Initialize tiles to WORKSPACE_INVALID
            for (size_t i = 0; i < images.size(); i++) {
                images[i].workspaceID = WORKSPACE_INVALID;
            }

            // Scan through workspaces lower than methodStartID until we wrap
            for (size_t i = 1; i < images.size() / 2; ++i) {
                currentID = getWorkspaceIDNameFromString(selector + "-" + std::to_string(i)).id;
                if (currentID >= firstID)
                    break;
                backtracked++;
                firstID = currentID;
            }

            // Scan through workspaces higher than methodStartID
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

            for (size_t i = 1; i < (size_t)(SIDE_LENGTH * SIDE_LENGTH); ++i) {
                auto& image = images[i];
                currentID   = getWorkspaceIDNameFromString(selector + "+" + std::to_string(i)).id;
                if (currentID <= methodStartID)
                    break;
                image.workspaceID = currentID;
            }

            pMonitor->m_activeWorkspace = startedOn;
        }
    }

    g_pHyprRenderer->makeEGLCurrent();

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D tileSize       = {pMonitor->m_size.x / cols, pMonitor->m_size.y / rows};
    Vector2D tileRenderSize = {(pMonitor->m_size.x - GAP_WIDTH * pMonitor->m_scale * (cols - 1)) / cols,
                               (pMonitor->m_size.y - GAP_WIDTH * pMonitor->m_scale * (rows - 1)) / rows};
    CBox     monbox{0, 0, tileSize.x * 2, tileSize.y * 2};

    if (!ENABLE_LOWRES)
        monbox = {{0, 0}, pMonitor->m_pixelSize};

    int          currentid = 0;

    PHLWORKSPACE openSpecial = PMONITOR->m_activeSpecialWorkspace;
    if (openSpecial)
        PMONITOR->m_activeSpecialWorkspace.reset();

    g_pHyprRenderer->m_bBlockSurfaceFeedback = true;

    startedOn->m_visible = false;

    for (size_t i = 0; i < images.size(); ++i) {
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

        // Calculate centering offsets
        int tilesInLastRow = images.size() % cols;
        if (tilesInLastRow == 0) tilesInLastRow = cols;
        int lastRow = (images.size() - 1) / cols;
        int actualRows = lastRow + 1;
        int row = i / cols;

        // Horizontal centering for partial last row
        double offsetX = 0;
        if (row == lastRow && tilesInLastRow < cols) {
            offsetX = (cols - tilesInLastRow) * (tileRenderSize.x + GAP_WIDTH) / 2.0;
        }

        // Vertical centering when tiles don't fill all rows
        double offsetY = 0;
        if (actualRows < rows) {
            offsetY = (rows - actualRows) * (tileRenderSize.y + GAP_WIDTH) / 2.0;
        }

        double monAspect = pMonitor->m_size.x / pMonitor->m_size.y;
        CBox   cellBox   = {offsetX + (i % cols) * tileRenderSize.x + (i % cols) * GAP_WIDTH,
                             offsetY + row * tileRenderSize.y + row * GAP_WIDTH,
                             tileRenderSize.x, tileRenderSize.y};
        CBox   fitBox    = aspectCorrectBox(tileRenderSize.x, tileRenderSize.y, monAspect);
        image.box = {cellBox.x + fitBox.x, cellBox.y + fitBox.y, fitBox.w, fitBox.h};

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;
        g_pHyprRenderer->endRender();
    }

    // Generate workspace number labels - ensure EGL context is current for GL calls
    g_pHyprRenderer->makeEGLCurrent();
    const int LABEL_SIZE = 36;
    for (size_t i = 0; i < images.size(); ++i) {
        images[i].labelTex = makeShared<CTexture>();
        std::string labelText = std::to_string(images[i].workspaceID);
        renderLabel(images[i].labelTex, labelText, LABEL_SIZE);
    }

    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    PMONITOR->m_activeSpecialWorkspace = openSpecial;
    PMONITOR->m_activeWorkspace        = startedOn;
    startedOn->m_visible               = true;
    g_pDesktopAnimationManager->startAnimation(startedOn, CDesktopAnimationManager::ANIMATION_TYPE_IN, true, true);

    // zoom on the current workspace.
    // const auto& TILE = images[std::clamp(currentid, 0, SIDE_LENGTH * SIDE_LENGTH)];

    g_pAnimationManager->createAnimation(pMonitor->m_size * pMonitor->m_size / tileSize, size, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation((-(Vector2D{(currentid % cols) * tileSize.x, (currentid / cols) * tileSize.y}) * pMonitor->m_scale) *
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

    Cursor::overrideController->setOverride("left_ptr", Cursor::CURSOR_OVERRIDE_UNKNOWN);

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    auto onCursorMove = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;
    };

    auto onCursorSelect = [this](Event::SCallbackInfo& info) {
        if (closing)
            return;

        info.cancelled = true;

        selectHoveredWorkspace();

        close();
    };

    mouseMoveHook = Event::bus()->m_events.input.mouse.move.listen([onCursorMove](Vector2D, Event::SCallbackInfo& info) { onCursorMove(info); });
    touchMoveHook = Event::bus()->m_events.input.touch.motion.listen([onCursorMove](ITouch::SMotionEvent, Event::SCallbackInfo& info) { onCursorMove(info); });

    mouseButtonHook = Event::bus()->m_events.input.mouse.button.listen([onCursorSelect](IPointer::SButtonEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
    touchDownHook   = Event::bus()->m_events.input.touch.down.listen([onCursorSelect](ITouch::SDownEvent, Event::SCallbackInfo& info) { onCursorSelect(info); });
}

void COverview::selectHoveredWorkspace() {
    if (closing)
        return;

    // Check each tile's actual box for hit detection (handles centered partial rows)
    for (size_t i = 0; i < images.size(); ++i) {
        const auto& box = images[i].box;
        if (lastMousePosLocal.x >= box.x && lastMousePosLocal.x < box.x + box.w &&
            lastMousePosLocal.y >= box.y && lastMousePosLocal.y < box.y + box.h) {
            closeOnID = i;
            return;
        }
    }

    // Fallback: use grid calculation and clamp to nearest valid tile
    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    int x     = lastMousePosLocal.x / pMonitor->m_size.x * cols;
    int y     = lastMousePosLocal.y / pMonitor->m_size.y * rows;
    int idx   = x + y * cols;
    closeOnID = std::clamp(idx, 0, (int)images.size() - 1);
}

void COverview::redrawID(int id, bool forcelowres) {
    if (!pMonitor)
        return;

    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    id = std::clamp(id, 0, (int)images.size() - 1);

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D tileSize       = {pMonitor->m_size.x / cols, pMonitor->m_size.y / rows};
    Vector2D tileRenderSize = {(pMonitor->m_size.x - GAP_WIDTH * (cols - 1)) / cols,
                               (pMonitor->m_size.y - GAP_WIDTH * (rows - 1)) / rows};
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
    if (!pMonitor)
        return;
    for (size_t i = 0; i < images.size(); ++i) {
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

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D tileSize       = {SIZE.x / cols, SIZE.y / rows};
    Vector2D tileRenderSize = {(SIZE.x - GAP_WIDTH * (cols - 1)) / cols,
                               (SIZE.y - GAP_WIDTH * (rows - 1)) / rows};
    CBox texbox = CBox{(openedID % cols) * tileRenderSize.x + (openedID % cols) * GAP_WIDTH,
                       (openedID / cols) * tileRenderSize.y + (openedID / cols) * GAP_WIDTH, tileRenderSize.x, tileRenderSize.y}
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

    const int   ID = closeOnID == -1 ? openedID : closeOnID;

    const auto& TILE = images[std::clamp(ID, 0, (int)images.size() - 1)];

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D    tileSize = {pMonitor->m_size.x / cols, pMonitor->m_size.y / rows};

    size->warp();
    pos->warp();

    *size = pMonitor->m_size * pMonitor->m_size / tileSize;
    *pos  = (-(Vector2D{(double)(ID % cols) * tileSize.x, (double)(ID / cols) * tileSize.y}) * pMonitor->m_scale) * (pMonitor->m_size / tileSize);

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

    size->setCallbackOnEnd([](auto) { g_pEventLoopManager->doLater([] { g_pOverview.reset(); }); });
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

    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i].workspaceID != pMonitor->activeWorkspaceID())
            continue;

        openedID = i;
        break;
    }

    closeOnID = openedID;
    if (!closing)
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

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D tileSize       = {SIZE.x / cols, SIZE.y / rows};
    Vector2D tileRenderSize = {(SIZE.x - GAPSIZE * (cols - 1)) / cols,
                               (SIZE.y - GAPSIZE * (rows - 1)) / rows};

    g_pHyprOpenGL->clear(BG_COLOR.stripA());

    for (size_t i = 0; i < images.size(); ++i) {
        int x = i % cols;
        int y = i / cols;

        // Calculate centering offsets
        int tilesInLastRow = images.size() % cols;
        if (tilesInLastRow == 0) tilesInLastRow = cols;
        int lastRow = (images.size() - 1) / cols;
        int actualRows = lastRow + 1;

        // Horizontal centering for partial last row
        double offsetX = 0;
        if (y == lastRow && tilesInLastRow < cols) {
            offsetX = (cols - tilesInLastRow) * (tileRenderSize.x + GAPSIZE) / 2.0;
        }

        // Vertical centering when tiles don't fill all rows
        double offsetY = 0;
        if (actualRows < rows) {
            offsetY = (rows - actualRows) * (tileRenderSize.y + GAPSIZE) / 2.0;
        }

        double monAspect = SIZE.x / SIZE.y;
        CBox   cellBox   = {offsetX + x * tileRenderSize.x + x * GAPSIZE, offsetY + y * tileRenderSize.y + y * GAPSIZE, tileRenderSize.x, tileRenderSize.y};
        CBox   fitBox    = aspectCorrectBox(tileRenderSize.x, tileRenderSize.y, monAspect);
        CBox   texbox    = {cellBox.x + fitBox.x, cellBox.y + fitBox.y, fitBox.w, fitBox.h};
        texbox.scale(pMonitor->m_scale).translate(pos->value());
        texbox.round();
        CRegion damage{0, 0, INT16_MAX, INT16_MAX};
        g_pHyprOpenGL->renderTextureInternal(images[i].fb.getTexture(), texbox, {.damage = &damage, .a = 1.0});

        // Render workspace number label at top-right corner
        if (images[i].labelTex && images[i].labelTex->m_texID) {
            const float labelSize = 36 * pMonitor->m_scale;
            const float padding   = 8 * pMonitor->m_scale;
            CBox        labelBox  = {
                texbox.x + texbox.w - labelSize - padding,
                texbox.y + padding,
                labelSize,
                labelSize
            };
            labelBox.round();
            g_pHyprOpenGL->renderTextureInternal(images[i].labelTex, labelBox, {.damage = &damage, .a = 0.9f});
        }
    }
}

void COverview::renderLabel(SP<CTexture>& tex, const std::string& text, int size) {
    if (!tex)
        return;

    const auto PMONITOR = pMonitor.lock();
    if (!PMONITOR)
        return;

    const float scale      = PMONITOR->m_scale;
    const int   bufferSize = std::max(1, (int)(size * scale));

    const auto CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize, bufferSize);
    if (cairo_surface_status(CAIROSURFACE) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(CAIROSURFACE);
        return;
    }
    const auto CAIRO = cairo_create(CAIROSURFACE);

    // Clear the surface
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // Draw semi-transparent dark background circle
    cairo_set_source_rgba(CAIRO, 0.0, 0.0, 0.0, 0.7);
    cairo_arc(CAIRO, bufferSize / 2.0, bufferSize / 2.0, bufferSize / 2.0 - 2, 0, 2 * M_PI);
    cairo_fill(CAIRO);

    // Draw text using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, text.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string("sans bold");
    pango_font_description_set_size(fontDesc, (int)(size * 0.5 * scale * PANGO_SCALE));
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    cairo_set_source_rgba(CAIRO, 1.0, 1.0, 1.0, 1.0);

    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);

    const double xOffset = (bufferSize / 2.0 - logical_rect.width / PANGO_SCALE / 2.0);
    const double yOffset = (bufferSize / 2.0 - logical_rect.height / PANGO_SCALE / 2.0);

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);
    cairo_surface_flush(CAIROSURFACE);

    // Upload to OpenGL texture
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex->allocate();
    glBindTexture(GL_TEXTURE_2D, tex->m_texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize, bufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
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

    static auto* const* PDISTANCE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance")->getDataStaticPtr();

    const float         PERC               = closing ? std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0) : 1.0 - std::clamp(delta / (double)**PDISTANCE, 0.0, 1.0);
    const auto          WORKSPACE_FOCUS_ID = closing && closeOnID != -1 ? closeOnID : openedID;

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D            tileSize = {pMonitor->m_size.x / cols, pMonitor->m_size.y / rows};

    const auto          SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
    const auto          POSMAX  = (-(Vector2D{(double)(WORKSPACE_FOCUS_ID % cols) * tileSize.x, (double)(WORKSPACE_FOCUS_ID / cols) * tileSize.y}) * pMonitor->m_scale) *
        (pMonitor->m_size / tileSize);

    const auto SIZEMIN = pMonitor->m_size;
    const auto POSMIN  = Vector2D{0, 0};

    size->setValueAndWarp(lerp(SIZEMIN, SIZEMAX, PERC));
    pos->setValueAndWarp(lerp(POSMIN, POSMAX, PERC));
}

void COverview::onSwipeEnd() {
    if (closing || !m_isSwiping)
        return;

    int cols = SIDE_LENGTH;
    int rows = dynamicGrid ? gridRows : SIDE_LENGTH;
    Vector2D tileSize = {pMonitor->m_size.x / cols, pMonitor->m_size.y / rows};
    const auto SIZEMIN = pMonitor->m_size;
    const auto SIZEMAX = pMonitor->m_size * pMonitor->m_size / tileSize;
    const auto PERC    = (size->value() - SIZEMIN).x / (SIZEMAX - SIZEMIN).x;
    if (PERC > 0.5) {
        close();
        return;
    }
    *size = pMonitor->m_size;
    *pos  = {0, 0};

    size->setCallbackOnEnd([this](auto) { redrawAll(true); });

    swipeWasCommenced = false;
    m_isSwiping       = false;
}
