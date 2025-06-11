#include "Scrolling.hpp"

#include <algorithm>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Utils;

constexpr float MIN_COLUMN_WIDTH = 0.05F;
constexpr float MAX_COLUMN_WIDTH = 1.F;
constexpr float MIN_ROW_HEIGHT   = 0.1F;
constexpr float MAX_ROW_HEIGHT   = 1.F;

//
void SColumnData::add(PHLWINDOW w) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.emplace_back(makeShared<SScrollingWindowData>(w, self.lock(), 1.F / (float)(windowDatas.size() + 1)));
}

void SColumnData::add(SP<SScrollingWindowData> w) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.emplace_back(w);
    w->column     = self;
    w->windowSize = 1.F / (float)(windowDatas.size());
}

void SColumnData::remove(PHLWINDOW w) {
    const auto SIZE_BEFORE = windowDatas.size();
    std::erase_if(windowDatas, [&w](const auto& e) { return e->window == w; });

    if (SIZE_BEFORE == windowDatas.size() && SIZE_BEFORE > 0)
        return;

    float newMaxSize = 0.F;
    for (auto& wd : windowDatas) {
        newMaxSize += wd->windowSize;
    }

    for (auto& wd : windowDatas) {
        wd->windowSize *= 1.F / newMaxSize;
    }

    if (windowDatas.empty() && workspace)
        workspace->remove(self.lock());
}

void SColumnData::up(SP<SScrollingWindowData> w) {
    for (size_t i = 1; i < windowDatas.size(); ++i) {
        if (windowDatas[i] != w)
            continue;

        std::swap(windowDatas[i], windowDatas[i - 1]);
    }
}

void SColumnData::down(SP<SScrollingWindowData> w) {
    for (size_t i = 0; i < windowDatas.size() - 1; ++i) {
        if (windowDatas[i] != w)
            continue;

        std::swap(windowDatas[i], windowDatas[i + 1]);
    }
}

SP<SScrollingWindowData> SColumnData::next(SP<SScrollingWindowData> w) {
    for (size_t i = 0; i < windowDatas.size() - 1; ++i) {
        if (windowDatas[i] != w)
            continue;

        return windowDatas[i + 1];
    }

    return nullptr;
}

SP<SScrollingWindowData> SColumnData::prev(SP<SScrollingWindowData> w) {
    for (size_t i = 1; i < windowDatas.size(); ++i) {
        if (windowDatas[i] != w)
            continue;

        return windowDatas[i - 1];
    }

    return nullptr;
}

bool SColumnData::has(PHLWINDOW w) {
    return std::ranges::find_if(windowDatas, [w](const auto& e) { return e->window == w; }) != windowDatas.end();
}

SP<SColumnData> SWorkspaceData::add() {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("plugin:hyprscrolling:column_width");
    auto              col       = columns.emplace_back(makeShared<SColumnData>(self.lock()));
    col->self                   = col;
    col->columnWidth            = *PCOLWIDTH;
    return col;
}

SP<SColumnData> SWorkspaceData::add(size_t after) {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("plugin:hyprscrolling:column_width");
    auto              col       = makeShared<SColumnData>(self.lock());
    col->self                   = col;
    col->columnWidth            = *PCOLWIDTH;
    columns.insert(columns.begin() + after + 1, col);
    return col;
}

int64_t SWorkspaceData::idx(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] == c)
            return i;
    }

    return -1;
}

void SWorkspaceData::remove(SP<SColumnData> c) {
    std::erase(columns, c);
}

SP<SColumnData> SWorkspaceData::next(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == columns.size() - 1)
            return nullptr;

        return columns[i + 1];
    }

    return nullptr;
}

SP<SColumnData> SWorkspaceData::prev(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == 0)
            return nullptr;

        return columns[i - 1];
    }

    return nullptr;
}

void SWorkspaceData::centerCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    PHLMONITOR        PMONITOR    = workspace->m_monitor.lock();
    double            currentLeft = 0;
    const auto        USABLE      = layout->usableAreaFor(PMONITOR);

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = currentLeft - (USABLE.w - ITEM_WIDTH) / 2.F;
            return;
        }
    }
}

void SWorkspaceData::fitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    PHLMONITOR        PMONITOR    = workspace->m_monitor.lock();
    double            currentLeft = 0;
    const auto        USABLE      = layout->usableAreaFor(PMONITOR);

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = std::clamp((double)leftOffset, currentLeft - USABLE.w + ITEM_WIDTH, currentLeft);
            return;
        }
    }
}

SP<SColumnData> SWorkspaceData::atCenter() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    PHLMONITOR        PMONITOR    = workspace->m_monitor.lock();
    double            currentLeft = leftOffset;
    const auto        USABLE      = layout->usableAreaFor(PMONITOR);

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        currentLeft += ITEM_WIDTH;

        if (currentLeft >= PMONITOR->m_size.x / 2.0 - 2)
            return COL;
    }

    return nullptr;
}

void SWorkspaceData::recalculate() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    if (!workspace || !workspace) {
        Debug::log(ERR, "[scroller] broken internal state on workspace data");
        return;
    }

    leftOffset = std::clamp((double)leftOffset, 0.0, maxWidth());

    const auto   MAX_WIDTH = maxWidth();

    PHLMONITOR   PMONITOR = workspace->m_monitor.lock();

    const CBox   USABLE = layout->usableAreaFor(PMONITOR);

    double       currentLeft = 0;
    const double cameraLeft  = MAX_WIDTH < USABLE.w ? std::round((MAX_WIDTH - USABLE.w) / 2.0) : leftOffset; // layout pixels

    for (const auto& COL : columns) {
        double       currentTop = 0.0;
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        for (const auto& WINDOW : COL->windowDatas) {
            WINDOW->layoutBox =
                CBox{currentLeft, currentTop, ITEM_WIDTH, WINDOW->windowSize * USABLE.h}.translate(PMONITOR->m_position + PMONITOR->m_reservedTopLeft + Vector2D{-cameraLeft, 0.0});

            currentTop += WINDOW->windowSize * USABLE.h;

            layout->applyNodeDataToWindow(WINDOW, false);
        }

        currentLeft += ITEM_WIDTH;
    }
}

double SWorkspaceData::maxWidth() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    PHLMONITOR        PMONITOR    = workspace->m_monitor.lock();
    double            currentLeft = 0;
    const auto        USABLE      = layout->usableAreaFor(PMONITOR);

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        currentLeft += ITEM_WIDTH;
    }

    return currentLeft;
}

bool SWorkspaceData::visible(SP<SColumnData> c) {
    const auto USABLE    = layout->usableAreaFor(workspace->m_monitor.lock());
    float      totalLeft = 0;
    for (const auto& col : columns) {
        if (col == c)
            return (totalLeft >= leftOffset && totalLeft < leftOffset + USABLE.w) ||
                (totalLeft + col->columnWidth * USABLE.w >= leftOffset && totalLeft + col->columnWidth * USABLE.w < leftOffset + USABLE.w);

        totalLeft += col->columnWidth * USABLE.w;
    }

    return false;
}

void CScrollingLayout::applyNodeDataToWindow(SP<SScrollingWindowData> data, bool force) {
    PHLMONITOR   PMONITOR;
    PHLWORKSPACE PWORKSPACE;

    if (!data || !data->column || !data->column->workspace) {
        if (!data->overrideWorkspace) {
            Debug::log(ERR, "[scroller] broken internal state on workspace (1)");
            return;
        }

        PMONITOR   = data->overrideWorkspace->m_monitor.lock();
        PWORKSPACE = data->overrideWorkspace.lock();
    } else {
        PMONITOR   = data->column->workspace->workspace->m_monitor.lock();
        PWORKSPACE = data->column->workspace->workspace.lock();
    }

    if (!PMONITOR || !PWORKSPACE) {
        Debug::log(ERR, "[scroller] broken internal state on workspace (2)");
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(data->layoutBox.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(data->layoutBox.x + data->layoutBox.w, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(data->layoutBox.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(data->layoutBox.y + data->layoutBox.h, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    const auto PWINDOW = data->window.lock();
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE);

    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", (uintptr_t)data.get(), PWINDOW);
        onWindowRemovedTiling(PWINDOW);
        return;
    }

    if (PWINDOW->isFullscreen())
        return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto PGAPSINDATA  = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSIN      = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
    auto* const PGAPSOUT     = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto        gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);
    CBox        nodeBox = data->layoutBox;
    nodeBox.round();

    PWINDOW->m_size     = nodeBox.size();
    PWINDOW->m_position = nodeBox.pos();

    PWINDOW->updateWindowDecos();

    auto       calcPos  = PWINDOW->m_position;
    auto       calcSize = PWINDOW->m_size;

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), (double)(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), (double)(DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    if (PWINDOW->m_isPseudotiled) {
        // Calculate pseudo
        float scale = 1;

        // adjust if doesnt fit
        if (PWINDOW->m_pseudoSize.x > calcSize.x || PWINDOW->m_pseudoSize.y > calcSize.y) {
            if (PWINDOW->m_pseudoSize.x > calcSize.x) {
                scale = calcSize.x / PWINDOW->m_pseudoSize.x;
            }

            if (PWINDOW->m_pseudoSize.y * scale > calcSize.y) {
                scale = calcSize.y / PWINDOW->m_pseudoSize.y;
            }

            auto DELTA = calcSize - PWINDOW->m_pseudoSize * scale;
            calcSize   = PWINDOW->m_pseudoSize * scale;
            calcPos    = calcPos + DELTA / 2.f; // center
        } else {
            auto DELTA = calcSize - PWINDOW->m_pseudoSize;
            calcPos    = calcPos + DELTA / 2.f; // center
            calcSize   = PWINDOW->m_pseudoSize;
        }
    }

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (PWINDOW->onSpecialWorkspace() && !PWINDOW->isFullscreen()) {
        // if special, we adjust the coords a bit
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("dwindle:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realSize     = wb.size();
        *PWINDOW->m_realPosition = wb.pos();
    }

    if (force) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

void CScrollingLayout::onEnable() {
  m_configCallback = g_pHookSystem->hookDynamic("configReloaded", 
      [this](void* handle, SCallbackInfo& info, std::any param) { this->loadConfig(); });
  loadConfig();
  for (auto const& w : g_pCompositor->m_windows) {
    if (w->m_isFloating || !w->m_isMapped || w->isHidden())
      continue;
    onWindowCreatedTiling(w);
  }
}

void CScrollingLayout::onDisable() {
    m_workspaceDatas.clear();
    m_configCallback.reset();
}

void CScrollingLayout::loadConfig() {
  m_config.configuredWidths.clear();
  m_config.focus_fit_method = *CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
  m_config.special_scale_factor = *CConfigValue<Hyprlang::FLOAT>("dwindle:special_scale_factor");
  m_config.fullscreen_on_one = (*CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:fullscreen_on_one_column") == "true");
  static const auto PCONFWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:explicit_column_widths");
  CConstVarList widths(*PCONFWIDTHS, 0, ',');
  for (auto& w : widths) {
    try { m_config.configuredWidths.emplace_back(std::stof(std::string{w})); }
    catch (...) { Debug::log(ERR, "scrolling: Failed to parse width {} as float", w); }
  }
}

void CScrollingLayout::onWindowCreatedTiling(PHLWINDOW window, eDirection direction) {
    auto workspaceData = dataFor(window->m_workspace);

    if (!workspaceData) {
        Debug::log(LOG, "[scrolling] No workspace data yet, creating");
        workspaceData       = m_workspaceDatas.emplace_back(makeShared<SWorkspaceData>(window->m_workspace, this));
        workspaceData->self = workspaceData;
    }

    auto droppingOn = g_pCompositor->m_lastWindow.lock();

    if (droppingOn == window)
        droppingOn = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS);

    SP<SScrollingWindowData> droppingData   = droppingOn ? dataFor(droppingOn) : nullptr;
    SP<SColumnData>          droppingColumn = droppingData ? droppingData->column.lock() : nullptr;

    Debug::log(LOG, "[scrolling] new window {:x}, droppingColumn: {:x}, columns before: {}", (uintptr_t)window.get(), (uintptr_t)droppingColumn.get(),
               workspaceData->columns.size());

    if (!droppingColumn) {
        auto col = workspaceData->add();
        col->add(window);
        workspaceData->fitCol(col);
    } else {
        if (window->m_draggingTiled) {
            droppingColumn->add(window);
            workspaceData->fitCol(droppingColumn);
        } else {
            auto idx = workspaceData->idx(droppingColumn);
            auto col = idx == -1 ? workspaceData->add() : workspaceData->add(idx);
            col->add(window);
            workspaceData->fitCol(col);
        }
    }

    workspaceData->recalculate();
}

void CScrollingLayout::onWindowRemovedTiling(PHLWINDOW window) {
    const auto DATA = dataFor(window);

    if (!DATA)
        return;

    const auto WS = DATA->column->workspace.lock();

    if (!WS->next(DATA->column.lock())) {
        // move the view if this is the last column
        const auto USABLE = usableAreaFor(window->m_monitor.lock());
        WS->leftOffset -= USABLE.w * DATA->column->columnWidth;
    }

    DATA->column->remove(window);

    WS->recalculate();

    if (!DATA->column) {
        // column got removed, let's ensure we don't leave any cringe extra space
        const auto USABLE = usableAreaFor(window->m_monitor.lock());
        WS->leftOffset    = std::clamp((double)WS->leftOffset, 0.0, std::max(WS->maxWidth() - USABLE.w, 1.0));
    }
}

bool CScrollingLayout::isWindowTiled(PHLWINDOW window) {
    const auto DATA = dataFor(window);

    return DATA;
}

void CScrollingLayout::recalculateMonitor(const MONITORID& id) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(id);
    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
        return;

    const auto DATA = dataFor(PMONITOR->m_activeWorkspace);

    if (!DATA)
        return;

    DATA->recalculate();
}

void CScrollingLayout::recalculateWindow(PHLWINDOW window) {
    if (!window->m_workspace)
        return;

    const auto DATA = dataFor(window->m_workspace);

    if (!DATA)
        return;

    DATA->recalculate();
}

void CScrollingLayout::onBeginDragWindow() {
    IHyprLayout::onBeginDragWindow();
}

void CScrollingLayout::resizeActiveWindow(const Vector2D& delta, eRectCorner corner, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_lastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto DATA = dataFor(PWINDOW);

    if (!DATA) {
        *PWINDOW->m_realSize =
            (PWINDOW->m_realSize->goal() + delta)
                .clamp(PWINDOW->m_windowData.minSize.valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), PWINDOW->m_windowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    if (corner == CORNER_NONE)
        return;

    if (!DATA->column || !DATA->column->workspace || !DATA->column->workspace->workspace || !DATA->column->workspace->workspace->m_monitor)
        return;

    const auto USABLE        = usableAreaFor(DATA->column->workspace->workspace->m_monitor.lock());
    const auto DELTA_AS_PERC = delta / USABLE.size();

    const auto CURR_COLUMN = DATA->column.lock();
    const auto NEXT_COLUMN = DATA->column->workspace->next(CURR_COLUMN);
    const auto PREV_COLUMN = DATA->column->workspace->prev(CURR_COLUMN);

    switch (corner) {
        case CORNER_BOTTOMLEFT:
        case CORNER_TOPLEFT: {
            if (!PREV_COLUMN)
                break;

            PREV_COLUMN->columnWidth = std::clamp(PREV_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }
        case CORNER_BOTTOMRIGHT:
        case CORNER_TOPRIGHT: {
            if (!NEXT_COLUMN)
                break;

            NEXT_COLUMN->columnWidth = std::clamp(NEXT_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }

        default: break;
    }

    if (DATA->column->windowDatas.size() > 1) {
        const auto CURR_WD = DATA;
        const auto NEXT_WD = DATA->column->next(DATA);
        const auto PREV_WD = DATA->column->prev(DATA);

        switch (corner) {
            case CORNER_BOTTOMLEFT:
            case CORNER_BOTTOMRIGHT: {
                if (!NEXT_WD)
                    break;

                if (NEXT_WD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(delta.y / USABLE.h), (-CURR_WD->windowSize + MIN_ROW_HEIGHT), (NEXT_WD->windowSize - MIN_ROW_HEIGHT));

                NEXT_WD->windowSize = std::clamp(NEXT_WD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_WD->windowSize = std::clamp(CURR_WD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }
            case CORNER_TOPLEFT:
            case CORNER_TOPRIGHT: {
                if (!PREV_WD)
                    break;

                if (PREV_WD->windowSize <= MIN_ROW_HEIGHT && delta.y <= 0 || CURR_WD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(delta.y / USABLE.h), -(PREV_WD->windowSize - MIN_ROW_HEIGHT), (CURR_WD->windowSize - MIN_ROW_HEIGHT));

                PREV_WD->windowSize = std::clamp(PREV_WD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_WD->windowSize = std::clamp(CURR_WD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }

            default: break;
        }
    }

    DATA->column->workspace->recalculate();
}

void CScrollingLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_monitor.lock();
    const auto PWORKSPACE = pWindow->m_workspace;

    // save position and size if floating
    if (pWindow->m_isFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_lastFloatingSize     = pWindow->m_realSize->goal();
        pWindow->m_lastFloatingPosition = pWindow->m_realPosition->goal();
        pWindow->m_position             = pWindow->m_realPosition->goal();
        pWindow->m_size                 = pWindow->m_realSize->goal();
    }

    const auto PNODE = dataFor(pWindow);

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one

        if (PNODE)
            applyNodeDataToWindow(PNODE, false);
        else {
            // get back its' dimensions from position and size
            *pWindow->m_realPosition = pWindow->m_lastFloatingPosition;
            *pWindow->m_realSize     = pWindow->m_lastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            *pWindow->m_realPosition = PMONITOR->m_position;
            *pWindow->m_realSize     = PMONITOR->m_size;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SP<SScrollingWindowData> fakeNode = makeShared<SScrollingWindowData>(pWindow, nullptr);
            fakeNode->window                  = pWindow;
            fakeNode->layoutBox = {PMONITOR->m_position + PMONITOR->m_reservedTopLeft, PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight};
            pWindow->m_size     = fakeNode->layoutBox.size();
            fakeNode->ignoreFullscreenChecks = true;
            fakeNode->overrideWorkspace      = pWindow->m_workspace;

            applyNodeDataToWindow(fakeNode, false);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CScrollingLayout::centerOrFit(const SP<SWorkspaceData> ws, const SP<SColumnData> col)
{
  if (m_config.focus_fit_method == 1) {
    ws->fitCol(col);
  } else {
    ws->centerCol(col);
  }
}

void CScrollingLayout::move(SP<SWorkspaceData> ws, std::string arg)
{
  if (!ws)
    return;

  const auto w = dataFor(g_pCompositor->m_lastWindow.lock());
  PHLWINDOW focus = nullptr;
  if (arg == "+col" || arg == "col") {
    if(!w)
      return;
    const auto col = ws->next(w->column.lock());
    if (!col) {
      ws->leftOffset = ws->maxWidth();
    } else {
      centerOrFit(ws, col);
      focus = col->windowDatas.front()->window.lock();
    }
  } else if (arg == "-col") {
    if (!w) {
      if (ws->leftOffset <= ws->maxWidth() && ws->columns.size() > 0) {
        ws->centerCol(ws->columns.back());
        focus = ws->columns.back()->windowDatas.back()->window.lock();
      }
    } else {
      const auto col = ws->prev(w->column.lock());
      if (!col)
        return;
      centerOrFit(ws, col);
      focus = col->windowDatas.back()->window.lock();
    }
  }
  else {
    const auto PLUSMINUS = getPlusMinusKeywordResult(arg, 0);
    if (!PLUSMINUS.has_value())
      return;
    ws->leftOffset -= *PLUSMINUS;
    auto col = ws->atCenter(); 
    focus = (col ? (*col->windowDatas.begin())->window.lock() : nullptr);
  }
  ws->recalculate();
  g_pCompositor->focusWindow(focus);
}

void CScrollingLayout::fit(CVarList args) {
  const auto w = dataFor(g_pCompositor->m_lastWindow.lock());
  const auto ws = dataFor(w->window->m_workspace);
  if (!w || !ws || ws->columns.size() == 0)
    return;
  if (args[1] == "active") {
    const auto USABLE = usableAreaFor(ws->workspace->m_monitor.lock());
    w->column->columnWidth = 1.F;
    ws->leftOffset = 0;
    for (size_t i = 0; i < ws->columns.size(); ++i) {
      if (ws->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
        break;
      ws->leftOffset += USABLE.w * ws->columns[i]->columnWidth;
    }
    w->column->workspace->recalculate();
  } else if (args[1] == "all") {
    // fit all columns on screen
    const size_t LEN = ws->columns.size();
    for (const auto& c : ws->columns) {
      c->columnWidth = 1.F / (float)LEN;
    }
    ws->recalculate();
  } else if (args[1] == "toend") {
    // fit all columns on screen that start from the current and end on the last
    bool   begun   = false;
    size_t foundAt = 0;
    for (size_t i = 0; i < ws->columns.size(); ++i) {
      if (!begun && !ws->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
        continue;
      if (!begun) {
        begun   = true;
        foundAt = i;
      }
      ws->columns[i]->columnWidth = 1.F / (float)(ws->columns.size() - i);
    }
    if (!begun)
      return;
    
    const auto USABLE = usableAreaFor(ws->workspace->m_monitor.lock());
    ws->leftOffset = 0;
    for (size_t i = 0; i < foundAt; ++i) {
      ws->leftOffset += USABLE.w * ws->columns[i]->columnWidth;
    }
    ws->recalculate();
  } else if (args[1] == "tobeg") {
    // fit all columns on screen that start from the current and end on the last
    bool begun   = false;
    size_t foundAt = 0;
    for (int64_t i = (int64_t)ws->columns.size() - 1; i >= 0; --i) {
      if (!begun && !ws->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
        continue;
      if (!begun) {
        begun   = true;
        foundAt = i;
      }
      ws->columns[i]->columnWidth = 1.F / (float)(foundAt + 1);
    }
    if (!begun)
      return;
    ws->leftOffset = 0;
    ws->recalculate();
  } else if (args[1] == "visible") {
    // fit all columns on screen that start from the current and end on the last
    bool    begun   = false;
    size_t  foundAt = 0;
    std::vector<SP<SColumnData>> visible;
    for (size_t i = 0; i < ws->columns.size(); ++i) {
      if (!begun && !ws->visible(ws->columns[i]))
        continue;
      if (!begun) {
        begun   = true;
        foundAt = i;
      }
      if (!ws->visible(ws->columns[i]))
        break;
      visible.emplace_back(ws->columns[i]);
    }
    if (!begun)
      return;
    ws->leftOffset = 0;
    if (foundAt != 0) {
      const auto USABLE = usableAreaFor(ws->workspace->m_monitor.lock());
      for (size_t i = 0; i < foundAt; ++i) {
        ws->leftOffset += USABLE.w * ws->columns[i]->columnWidth;
      }
    }
    for (const auto& v : visible) {
      v->columnWidth = 1.F / (float)visible.size();
    }
    ws->recalculate();
  }
}

void CScrollingLayout::colresize(SP<SWorkspaceData> ws, CVarList args) {
  const auto w = dataFor(g_pCompositor->m_lastWindow.lock());
  if (!ws)
    return;
  if (args[1] == "all") {
    float abs = 0;
    try { abs = std::stof(args[2]); }
    catch (...) { return; }
    for (const auto& col : ws->columns) {
      col->columnWidth = abs;
    }
    ws->recalculate();
  }
  CScopeGuard x([w] {
    w->column->columnWidth = std::clamp(w->column->columnWidth, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
    w->column->workspace->fitCol(w->column.lock());
    w->column->workspace->recalculate();
  });
  if (args[1][0] == '+' || args[1][0] == '-') {
    if (args[1] == "+conf") {
      auto it = std::upper_bound(m_config.configuredWidths.begin(), m_config.configuredWidths.end(), w->column->columnWidth);
      w->column->columnWidth = (it != m_config.configuredWidths.end()) ? *it : m_config.configuredWidths[0];
    } else if (args[1] == "-conf") {
      auto it = std::lower_bound(m_config.configuredWidths.begin(), m_config.configuredWidths.end(), w->column->columnWidth);
      w->column->columnWidth = (it != m_config.configuredWidths.begin()) ? *(--it) : m_config.configuredWidths.back();
    } else {
      const auto PLUSMINUS = getPlusMinusKeywordResult(args[1], 0);
      Debug::log(ERR, "Got arg: {} and val: {}",args[1], *PLUSMINUS);
      if (!PLUSMINUS.has_value())
        return;
      if(*PLUSMINUS > 1)
      {
        // TODO calc pixels to % of screen
      }
      w->column->columnWidth += *PLUSMINUS;
    }
  } else {
    float abs = 0;
    try { 
      abs = std::stof(args[1]);
    }
    catch (...) { return; }
    w->column->columnWidth = abs;
  }
  centerOrFit(ws, w->column.lock());
  ws->recalculate();
}

void CScrollingLayout::focus(const std::string &arg)
{
  const auto w = dataFor(g_pCompositor->m_lastWindow.lock());
  if (!w || arg.empty())
    return;
  switch (arg[0]) {
    case 'u':
    case 't': {
      auto PREV = w->column->prev(w);
      if (!PREV)
        PREV = w->column->windowDatas.back();
      g_pCompositor->focusWindow(PREV->window.lock());
      break;
    }
    case 'b':
    case 'd': {
      auto NEXT = w->column->next(w);
      if (!NEXT)
        NEXT = w->column->windowDatas.front();
      g_pCompositor->focusWindow(NEXT->window.lock());
      break;
    }
    case 'l': {
      auto PREV = w->column->workspace->prev(w->column.lock());
      if (!PREV)
        PREV = w->column->workspace->columns.back();
      g_pCompositor->focusWindow(PREV->windowDatas.front()->window.lock());
      centerOrFit(w->column->workspace.lock(), PREV);
      w->column->workspace->recalculate();
      break;
    }
    case 'r': {
      auto NEXT = w->column->workspace->next(w->column.lock());
      if (!NEXT)
        NEXT = w->column->workspace->columns.front();
      g_pCompositor->focusWindow(NEXT->windowDatas.front()->window.lock());
      centerOrFit(w->column->workspace.lock(), NEXT);
      w->column->workspace->recalculate();
      break;
    }
    default: return;
  }
}

void CScrollingLayout::promote()
{
  const auto w = dataFor(g_pCompositor->m_lastWindow.lock());
  if (!w)
    return;
  auto idx = w->column->workspace->idx(w->column.lock());
  auto col = idx == -1 ? w->column->workspace->add() : w->column->workspace->add(idx);
  w->column->remove(w->window.lock());
  col->add(w);
}

std::any CScrollingLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
  const auto args = CVarList(message, 0, 's');

#define ELIF_CHAIN(str, func_call) \
  if (args[0] == str) { \
    func_call; \
  } else 

  ELIF_CHAIN("move", move(currentWorkspaceData(), args[1]))
  ELIF_CHAIN("colresize", colresize(currentWorkspaceData(), args))
  ELIF_CHAIN("movewindowto", moveWindowTo(g_pCompositor->m_lastWindow.lock(), args[1], false))
  ELIF_CHAIN("fit", fit(args))
  ELIF_CHAIN("focus", focus(args[1]))
  ELIF_CHAIN("promote", promote())
  ELIF_CHAIN("swap", swap(args[1]))
  {
    // note: never forget the default... this to way to long to find..
  }
#undef ELIF_CHAIN
  return {};
}

SWindowRenderLayoutHints CScrollingLayout::requestRenderHints(PHLWINDOW a) {
    return {};
}

void CScrollingLayout::switchWindows(PHLWINDOW a, PHLWINDOW b) {
    ;
}

void CScrollingLayout::moveWindowTo(PHLWINDOW w, const std::string& dir, bool silent) {
    const auto DATA = dataFor(w);

    if (!DATA)
        return;

    const auto WS = DATA->column->workspace.lock();

    if (dir == "l") {
        const auto COL = WS->prev(DATA->column.lock());
        if (!COL)
            return;

        DATA->column->remove(w);
        COL->add(DATA);
        WS->centerCol(COL);
    } else if (dir == "r") {
        const auto COL = WS->next(DATA->column.lock());

        DATA->column->remove(w);

        if (!COL) {
            // make a new one
            const auto NEWCOL = WS->add();
            NEWCOL->add(DATA);
            WS->centerCol(NEWCOL);
        } else {
            COL->add(DATA);
            WS->centerCol(COL);
        }

    } else if (dir == "t" || dir == "u")
        DATA->column->up(DATA);
    else if (dir == "b" || dir == "d")
        DATA->column->down(DATA);

    WS->recalculate();
}

void CScrollingLayout::alterSplitRatio(PHLWINDOW, float, bool) {
    ;
}

std::string CScrollingLayout::getLayoutName() {
    return "scrolling";
}

void CScrollingLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
    ;
}

Vector2D CScrollingLayout::predictSizeForNewWindowTiled() {
    return Vector2D{};
}

SP<SWorkspaceData> CScrollingLayout::dataFor(PHLWORKSPACE ws) {
    for (const auto& e : m_workspaceDatas) {
        if (e->workspace != ws)
            continue;

        return e;
    }

    return nullptr;
}

SP<SScrollingWindowData> CScrollingLayout::dataFor(PHLWINDOW w) {
    if (!w)
        return nullptr;

    for (const auto& e : m_workspaceDatas) {
        if (e->workspace != w->m_workspace)
            continue;

        for (const auto& c : e->columns) {
            for (const auto& d : c->windowDatas) {
                if (d->window != w)
                    continue;

                return d;
            }
        }
    }

    return nullptr;
}

SP<SWorkspaceData> CScrollingLayout::currentWorkspaceData() {
    if (!g_pCompositor->m_lastMonitor || !g_pCompositor->m_lastMonitor->m_activeWorkspace)
        return nullptr;

    // FIXME: special

    return dataFor(g_pCompositor->m_lastMonitor->m_activeWorkspace);
}

CBox CScrollingLayout::usableAreaFor(PHLMONITOR m) {
    return CBox{m->m_reservedTopLeft, m->m_size - m->m_reservedTopLeft - m->m_reservedBottomRight};
}

void CScrollingLayout::swap(const std::string &dir) {
    auto wd = dataFor(g_pCompositor->m_lastWindow.lock());
    auto wds = dataFor(g_pCompositor->m_lastMonitor->m_activeWorkspace);
    if(!wd || wd->column->windowDatas.size() == 0 || !wds)
      return;
    int offset = 0;
    if(dir == "l")
      offset -= 1;
    else if (dir == "r")
      offset += 1;
    else
      return;

    auto it = std::find(wds->columns.begin(), wds->columns.end(), wd->column);
    if(it != wds->columns.end())
    {
      auto i = std::distance(wds->columns.begin(), it);
      if(i >= 0 && i + offset < wds->columns.size())
      {
        // maybe focus the swapped out window? who knows.
        //g_pCompositor->focusWindow(wds->columns[i+offset]->windowDatas.front()->window.lock());
        std::swap(wds->columns[i], wds->columns[i+offset]);
        wds->centerCol(wds->columns[i+offset]);
        wds->recalculate();
      }
  }
}
