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

void SColumnData::add(PHLWINDOW w, int after) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.insert(windowDatas.begin() + after + 1, makeShared<SScrollingWindowData>(w, self.lock(), 1.F / (float)(windowDatas.size() + 1)));
}

void SColumnData::add(SP<SScrollingWindowData> w) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.emplace_back(w);
    w->column     = self;
    w->windowSize = 1.F / (float)(windowDatas.size());
}

void SColumnData::add(SP<SScrollingWindowData> w, int after) {
    for (auto& wd : windowDatas) {
        wd->windowSize *= (float)windowDatas.size() / (float)(windowDatas.size() + 1);
    }

    windowDatas.insert(windowDatas.begin() + after + 1, w);
    w->column     = self;
    w->windowSize = 1.F / (float)(windowDatas.size());
}

size_t SColumnData::idx(PHLWINDOW w) {
    for (size_t i = 0; i < windowDatas.size(); ++i) {
        if (windowDatas[i]->window == w)
            return i;
    }
    return 0;
}

size_t SColumnData::idxForHeight(float y) {
    for (size_t i = 0; i < windowDatas.size(); ++i) {
        if (windowDatas[i]->window->m_position.y < y)
            continue;
        return i - 1;
    }
    return windowDatas.size() - 1;
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
        break;
    }
}

void SColumnData::down(SP<SScrollingWindowData> w) {
    for (size_t i = 0; i < windowDatas.size() - 1; ++i) {
        if (windowDatas[i] != w)
            continue;

        std::swap(windowDatas[i], windowDatas[i + 1]);
        break;
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

SP<SColumnData> SWorkspaceData::add(int after) {
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

void SWorkspaceData::centerOrFitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");

    if (*PFITMETHOD == 1)
        fitCol(c);
    else
        centerCol(c);
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

void SWorkspaceData::recalculate(bool forceInstant) {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

    if (!workspace) {
        Debug::log(ERR, "[scroller] broken internal state on workspace data");
        return;
    }

    const auto   MAX_WIDTH = maxWidth();

    PHLMONITOR   PMONITOR = workspace->m_monitor.lock();

    const CBox   USABLE = layout->usableAreaFor(PMONITOR);

    double       currentLeft = 0;
    const double cameraLeft  = MAX_WIDTH < USABLE.w ? std::round((MAX_WIDTH - USABLE.w) / 2.0) : leftOffset; // layout pixels

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto&  COL        = columns[i];
        double       currentTop = 0.0;
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        for (const auto& WINDOW : COL->windowDatas) {
            WINDOW->layoutBox =
                CBox{currentLeft, currentTop, ITEM_WIDTH, WINDOW->windowSize * USABLE.h}.translate(PMONITOR->m_position + PMONITOR->m_reservedTopLeft + Vector2D{-cameraLeft, 0.0});

            currentTop += WINDOW->windowSize * USABLE.h;

            layout->applyNodeDataToWindow(WINDOW, forceInstant, i != columns.size() - 1, i != 0);
        }

        currentLeft += ITEM_WIDTH;
        if (currentLeft == USABLE.width)
            currentLeft++; // avoid ffm from "grabbing" the window on the right
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
        if (col == c) {
            const float colLeft   = totalLeft;
            const float colRight  = totalLeft + col->columnWidth * USABLE.w;
            const float viewLeft  = leftOffset;
            const float viewRight = leftOffset + USABLE.w;
            return colLeft < viewRight && viewLeft < colRight;
        }

        totalLeft += col->columnWidth * USABLE.w;
    }

    return false;
}

void CScrollingLayout::applyNodeDataToWindow(SP<SScrollingWindowData> data, bool force, bool hasWindowsRight, bool hasWindowsLeft) {
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
    const bool DISPLAYLEFT   = !hasWindowsLeft;
    const bool DISPLAYRIGHT  = !hasWindowsRight;
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

    if (PWINDOW->isFullscreen() && !data->ignoreFullscreenChecks)
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
    static const auto PCONFWIDTHS = CConfigValue<Hyprlang::STRING>("plugin:hyprscrolling:explicit_column_widths");

    m_configCallback = g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) {
        // bitch ass
        m_config.configuredWidths.clear();

        CConstVarList widths(*PCONFWIDTHS, 0, ',');
        for (auto& w : widths) {
            try {
                m_config.configuredWidths.emplace_back(std::stof(std::string{w}));
            } catch (...) { Debug::log(ERR, "scrolling: Failed to parse width {} as float", w); }
        }
    });

    m_focusCallback = g_pHookSystem->hookDynamic("activeWindow", [this](void* hk, SCallbackInfo& info, std::any param) {
        const auto PWINDOW = std::any_cast<PHLWINDOW>(param);

        if (!PWINDOW)
            return;

        static const auto PFOLLOW_FOCUS = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:follow_focus");

        if (!*PFOLLOW_FOCUS)
            return;

        if (!PWINDOW->m_workspace->isVisible())
            return;

        const auto DATA       = dataFor(PWINDOW->m_workspace);
        const auto WINDOWDATA = dataFor(PWINDOW);

        if (!DATA || !WINDOWDATA)
            return;

        DATA->fitCol(WINDOWDATA->column.lock());
        DATA->recalculate();
    });

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

void CScrollingLayout::onWindowCreatedTiling(PHLWINDOW window, eDirection direction) {
    if (m_columnMoveState.isMovingColumn && window->m_workspace->m_id == m_columnMoveState.targetWorkspaceID)
        return;

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
            if (droppingOn) {
                const auto IDX = droppingColumn->idx(droppingOn);
                const auto TOP = droppingOn->getWindowIdealBoundingBoxIgnoreReserved().middle().y > g_pInputManager->getMouseCoordsInternal().y;
                droppingColumn->add(window, TOP ? (IDX == 0 ? -1 : IDX - 1) : (IDX));
            } else
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
    const auto PWINDOW  = pWindow ? pWindow : g_pCompositor->m_lastWindow.lock();
    Vector2D   modDelta = delta;

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
        if (corner == CORNER_NONE) {
            if (!PREV_WD)
                corner = CORNER_BOTTOMRIGHT;
            else {
                corner = CORNER_TOPRIGHT;
                modDelta.y *= -1.0f;
            }
        }

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

                if (PREV_WD->windowSize <= MIN_ROW_HEIGHT && modDelta.y <= 0 || CURR_WD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(modDelta.y / USABLE.h), -(PREV_WD->windowSize - MIN_ROW_HEIGHT), (CURR_WD->windowSize - MIN_ROW_HEIGHT));

                PREV_WD->windowSize = std::clamp(PREV_WD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_WD->windowSize = std::clamp(CURR_WD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }

            default: break;
        }
    }

    DATA->column->workspace->recalculate(true);
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
            applyNodeDataToWindow(PNODE, false, false, false);
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

            applyNodeDataToWindow(fakeNode, false, false, false);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CScrollingLayout::focusWindowUpdate(PHLWINDOW pWindow) {
    if (!validMapped(pWindow)) {
        g_pCompositor->focusWindow(nullptr);
        return;
    }
    g_pCompositor->focusWindow(pWindow);
    const auto WINDOWDATA = dataFor(pWindow);
    if (WINDOWDATA) {
        if (auto col = WINDOWDATA->column.lock())
            col->lastFocusedWindow = WINDOWDATA;
    }
}

SP<SScrollingWindowData> CScrollingLayout::findBestNeighbor(SP<SScrollingWindowData> pCurrent, SP<SColumnData> pTargetCol) {
    if (!pCurrent || !pTargetCol || pTargetCol->windowDatas.empty())
        return nullptr;

    const double                          currentTop    = pCurrent->layoutBox.y;
    const double                          currentBottom = pCurrent->layoutBox.y + pCurrent->layoutBox.h;
    std::vector<SP<SScrollingWindowData>> overlappingWindows;
    for (const auto& candidate : pTargetCol->windowDatas) {
        const double candidateTop    = candidate->layoutBox.y;
        const double candidateBottom = candidate->layoutBox.y + candidate->layoutBox.h;
        const bool   overlaps        = (candidateTop < currentBottom) && (candidateBottom > currentTop);

        if (overlaps)
            overlappingWindows.emplace_back(candidate);
    }
    if (!overlappingWindows.empty()) {
        auto lastFocused = pTargetCol->lastFocusedWindow.lock();

        if (lastFocused) {
            auto it = std::ranges::find(overlappingWindows, lastFocused);
            if (it != overlappingWindows.end())
                return lastFocused;
        }

        auto topmost = std::ranges::min_element(overlappingWindows, std::less<>{}, [](const SP<SScrollingWindowData>& w) { return w->layoutBox.y; });
        return *topmost;
    }
    if (!pTargetCol->windowDatas.empty())
        return pTargetCol->windowDatas.front();
    return nullptr;
}

std::any CScrollingLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    static auto centerOrFit = [](const SP<SWorkspaceData> WS, const SP<SColumnData> COL) -> void {
        static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
        if (*PFITMETHOD == 1)
            WS->fitCol(COL);
        else
            WS->centerCol(COL);
    };

    const auto ARGS = CVarList(message, 0, ' ');
    if (ARGS[0] == "move") {
        const auto DATA = currentWorkspaceData();
        if (!DATA)
            return {};

        if (ARGS[1] == "+col" || ARGS[1] == "col") {
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());
            if (!WDATA)
                return {};

            const auto COL = DATA->next(WDATA->column.lock());
            if (!COL) {
                // move to max
                DATA->leftOffset = DATA->maxWidth();
                DATA->recalculate();
                focusWindowUpdate(nullptr);
                return {};
            }

            centerOrFit(DATA, COL);
            DATA->recalculate();

            focusWindowUpdate(COL->windowDatas.front()->window.lock());
            g_pCompositor->warpCursorTo(COL->windowDatas.front()->window.lock()->middle());

            return {};
        } else if (ARGS[1] == "-col") {
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());
            if (!WDATA) {
                if (DATA->leftOffset <= DATA->maxWidth() && DATA->columns.size() > 0) {
                    DATA->centerCol(DATA->columns.back());
                    DATA->recalculate();
                    focusWindowUpdate((DATA->columns.back()->windowDatas.back())->window.lock());
                    g_pCompositor->warpCursorTo((DATA->columns.back()->windowDatas.back())->window.lock()->middle());
                }

                return {};
            }

            const auto COL = DATA->prev(WDATA->column.lock());
            if (!COL)
                return {};

            centerOrFit(DATA, COL);
            DATA->recalculate();

            focusWindowUpdate(COL->windowDatas.back()->window.lock());
            g_pCompositor->warpCursorTo(COL->windowDatas.front()->window.lock()->middle());

            return {};
        }

        const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

        if (!PLUSMINUS.has_value())
            return {};

        DATA->leftOffset -= *PLUSMINUS;
        DATA->recalculate();

        const auto ATCENTER = DATA->atCenter();

        focusWindowUpdate(ATCENTER ? (*ATCENTER->windowDatas.begin())->window.lock() : nullptr);
    } else if (ARGS[0] == "colresize") {
        const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());

        if (!WDATA)
            return {};

        if (ARGS[1] == "all") {
            float abs = 0;
            try {
                abs = std::stof(ARGS[2]);
            } catch (...) { return {}; }

            for (const auto& c : WDATA->column->workspace->columns) {
                c->columnWidth = abs;
            }

            WDATA->column->workspace->recalculate();
            return {};
        }

        CScopeGuard x([WDATA] {
            WDATA->column->columnWidth = std::clamp(WDATA->column->columnWidth, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            WDATA->column->workspace->centerOrFitCol(WDATA->column.lock());
            WDATA->column->workspace->recalculate();
        });

        if (ARGS[1][0] == '+' || ARGS[1][0] == '-') {
            if (ARGS[1] == "+conf") {
                for (size_t i = 0; i < m_config.configuredWidths.size(); ++i) {
                    if (m_config.configuredWidths[i] < WDATA->column->columnWidth)
                        continue;

                    if (i == m_config.configuredWidths.size() - 1)
                        WDATA->column->columnWidth = m_config.configuredWidths[0];
                    else
                        WDATA->column->columnWidth = m_config.configuredWidths[i + 1];

                    break;
                }

                return {};
            } else if (ARGS[1] == "-conf") {
                for (size_t i = m_config.configuredWidths.size() - 1; i >= 0; --i) {
                    if (m_config.configuredWidths[i] > WDATA->column->columnWidth)
                        continue;

                    if (i == 0)
                        WDATA->column->columnWidth = m_config.configuredWidths[m_config.configuredWidths.size() - 1];
                    else
                        WDATA->column->columnWidth = m_config.configuredWidths[i - 1];

                    break;
                }

                return {};
            }

            const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

            if (!PLUSMINUS.has_value())
                return {};

            WDATA->column->columnWidth += *PLUSMINUS;
        } else {
            float abs = 0;
            try {
                abs = std::stof(ARGS[1]);
            } catch (...) { return {}; }

            WDATA->column->columnWidth = abs;
        }
    } else if (ARGS[0] == "movewindowto") {
        moveWindowTo(g_pCompositor->m_lastWindow.lock(), ARGS[1], false);
    } else if (ARGS[0] == "fit") {

        if (ARGS[1] == "active") {
            // fit the current column to 1.F
            const auto WDATA    = dataFor(g_pCompositor->m_lastWindow.lock());
            const auto WORKDATA = dataFor(g_pCompositor->m_lastWindow->m_workspace);

            if (!WDATA || !WORKDATA || WORKDATA->columns.size() == 0)
                return {};

            const auto USABLE = usableAreaFor(WORKDATA->workspace->m_monitor.lock());

            WDATA->column->columnWidth = 1.F;

            WORKDATA->leftOffset = 0;
            for (size_t i = 0; i < WORKDATA->columns.size(); ++i) {
                if (WORKDATA->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
                    break;

                WORKDATA->leftOffset += USABLE.w * WORKDATA->columns[i]->columnWidth;
            }

            WDATA->column->workspace->recalculate();
        } else if (ARGS[1] == "all") {
            // fit all columns on screen
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow->m_workspace);

            if (!WDATA || WDATA->columns.size() == 0)
                return {};

            const size_t LEN = WDATA->columns.size();
            for (const auto& c : WDATA->columns) {
                c->columnWidth = 1.F / (float)LEN;
            }

            WDATA->recalculate();
        } else if (ARGS[1] == "toend") {
            // fit all columns on screen that start from the current and end on the last
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow->m_workspace);

            if (!WDATA || WDATA->columns.size() == 0)
                return {};

            bool   begun   = false;
            size_t foundAt = 0;
            for (size_t i = 0; i < WDATA->columns.size(); ++i) {
                if (!begun && !WDATA->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                WDATA->columns[i]->columnWidth = 1.F / (float)(WDATA->columns.size() - i);
            }

            if (!begun)
                return {};

            const auto USABLE = usableAreaFor(WDATA->workspace->m_monitor.lock());

            WDATA->leftOffset = 0;
            for (size_t i = 0; i < foundAt; ++i) {
                WDATA->leftOffset += USABLE.w * WDATA->columns[i]->columnWidth;
            }

            WDATA->recalculate();
        } else if (ARGS[1] == "tobeg") {
            // fit all columns on screen that start from the current and end on the last
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow->m_workspace);

            if (!WDATA || WDATA->columns.size() == 0)
                return {};

            bool   begun   = false;
            size_t foundAt = 0;
            for (int64_t i = (int64_t)WDATA->columns.size() - 1; i >= 0; --i) {
                if (!begun && !WDATA->columns[i]->has(g_pCompositor->m_lastWindow.lock()))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                WDATA->columns[i]->columnWidth = 1.F / (float)(foundAt + 1);
            }

            if (!begun)
                return {};

            WDATA->leftOffset = 0;

            WDATA->recalculate();
        } else if (ARGS[1] == "visible") {
            // fit all columns on screen that start from the current and end on the last
            const auto WDATA = dataFor(g_pCompositor->m_lastWindow->m_workspace);

            if (!WDATA || WDATA->columns.size() == 0)
                return {};

            bool                         begun   = false;
            size_t                       foundAt = 0;
            std::vector<SP<SColumnData>> visible;
            for (size_t i = 0; i < WDATA->columns.size(); ++i) {
                if (!begun && !WDATA->visible(WDATA->columns[i]))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                if (!WDATA->visible(WDATA->columns[i]))
                    break;

                visible.emplace_back(WDATA->columns[i]);
            }

            if (!begun)
                return {};

            WDATA->leftOffset = 0;

            if (foundAt != 0) {
                const auto USABLE = usableAreaFor(WDATA->workspace->m_monitor.lock());

                for (size_t i = 0; i < foundAt; ++i) {
                    WDATA->leftOffset += USABLE.w * WDATA->columns[i]->columnWidth;
                }
            }

            for (const auto& v : visible) {
                v->columnWidth = 1.F / (float)visible.size();
            }

            WDATA->recalculate();
        }
    } else if (ARGS[0] == "focus") {
        const auto        WDATA       = dataFor(g_pCompositor->m_lastWindow.lock());
        static const auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");

        if (!WDATA || ARGS[1].empty())
            return {};

        switch (ARGS[1][0]) {
            case 'u':
            case 't': {
                auto PREV = WDATA->column->prev(WDATA);
                if (!PREV) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        PREV = WDATA->column->windowDatas.back();
                }

                focusWindowUpdate(PREV->window.lock());
                g_pCompositor->warpCursorTo(PREV->window.lock()->middle());
                break;
            }

            case 'b':
            case 'd': {
                auto NEXT = WDATA->column->next(WDATA);
                if (!NEXT) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        NEXT = WDATA->column->windowDatas.front();
                }

                focusWindowUpdate(NEXT->window.lock());
                g_pCompositor->warpCursorTo(NEXT->window.lock()->middle());
                break;
            }

            case 'l': {
                auto PREV = WDATA->column->workspace->prev(WDATA->column.lock());
                if (!PREV) {
                    if (*PNOFALLBACK) {
                        centerOrFit(WDATA->column->workspace.lock(), WDATA->column.lock());
                        WDATA->column->workspace->recalculate();
                        g_pCompositor->warpCursorTo(WDATA->window.lock()->middle());
                        break;
                    } else
                        PREV = WDATA->column->workspace->columns.back();
                }

                auto pTargetWindowData = findBestNeighbor(WDATA, PREV);
                if (pTargetWindowData) {
                    focusWindowUpdate(pTargetWindowData->window.lock());
                    centerOrFit(WDATA->column->workspace.lock(), PREV);
                    WDATA->column->workspace->recalculate();
                    g_pCompositor->warpCursorTo(pTargetWindowData->window.lock()->middle());
                }
                break;
            }

            case 'r': {
                auto NEXT = WDATA->column->workspace->next(WDATA->column.lock());
                if (!NEXT) {
                    if (*PNOFALLBACK) {
                        centerOrFit(WDATA->column->workspace.lock(), WDATA->column.lock());
                        WDATA->column->workspace->recalculate();
                        g_pCompositor->warpCursorTo(WDATA->window.lock()->middle());
                        break;
                    } else
                        NEXT = WDATA->column->workspace->columns.front();
                }

                auto pTargetWindowData = findBestNeighbor(WDATA, NEXT);
                if (pTargetWindowData) {
                    focusWindowUpdate(pTargetWindowData->window.lock());
                    centerOrFit(WDATA->column->workspace.lock(), NEXT);
                    WDATA->column->workspace->recalculate();
                    g_pCompositor->warpCursorTo(pTargetWindowData->window.lock()->middle());
                }
                break;
            }

            default: return {};
        }
    } else if (ARGS[0] == "promote") {
        const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());

        if (!WDATA)
            return {};

        auto idx = WDATA->column->workspace->idx(WDATA->column.lock());
        auto col = idx == -1 ? WDATA->column->workspace->add() : WDATA->column->workspace->add(idx);

        WDATA->column->remove(WDATA->window.lock());

        col->add(WDATA);

        WDATA->column->workspace->recalculate();
    } else if (ARGS[0] == "swapcol") {
        if (ARGS.size() < 2)
            return {};

        const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());
        if (!WDATA)
            return {};

        const auto CURRENT_COL = WDATA->column.lock();
        if (!CURRENT_COL)
            return {};

        const auto WS_DATA = CURRENT_COL->workspace.lock();
        if (!WS_DATA || WS_DATA->columns.size() < 2)
            return {};

        const int64_t current_idx = WS_DATA->idx(CURRENT_COL);
        const size_t  col_count   = WS_DATA->columns.size();

        if (current_idx == -1)
            return {};

        const std::string& direction  = ARGS[1];
        int64_t            target_idx = -1;

        if (direction == "l")
            target_idx = (current_idx == 0) ? (col_count - 1) : (current_idx - 1);
        else if (direction == "r")
            target_idx = (current_idx == (int64_t)col_count - 1) ? 0 : (current_idx + 1);
        else
            return {};

        std::swap(WS_DATA->columns[current_idx], WS_DATA->columns[target_idx]);
        WS_DATA->centerOrFitCol(CURRENT_COL);
        WS_DATA->recalculate();
    } else if (ARGS[0] == "movecoltoworkspace") {
        if (ARGS.size() < 2)
            return {};

        const auto WDATA = dataFor(g_pCompositor->m_lastWindow.lock());
        if (!WDATA)
            return {};

        const auto CURRENT_COL = WDATA->column.lock();
        if (!CURRENT_COL)
            return {};

        const auto SOURCE_WS_DATA = CURRENT_COL->workspace.lock();
        if (!SOURCE_WS_DATA)
            return {};

        const auto PMONITOR = g_pCompositor->m_lastWindow->m_monitor.lock();
        if (!PMONITOR)
            return {};

        PHLWORKSPACE       PWORKSPACE = nullptr;
        const std::string& arg        = ARGS[1];

        if (arg.starts_with("+") || arg.starts_with("-")) {
            try {
                const int offset             = std::stoi(arg);
                const int currentWorkspaceID = WDATA->window->m_workspace->m_id;
                const int targetWorkspaceID  = currentWorkspaceID + offset;

                if (targetWorkspaceID < 1)
                    return {};

                PWORKSPACE = g_pCompositor->getWorkspaceByID(targetWorkspaceID);
                if (!PWORKSPACE)
                    PWORKSPACE = g_pCompositor->createNewWorkspace(targetWorkspaceID, PMONITOR->m_id);
            } catch (...) { return {}; }
        } else if (arg == "special") {
            const int SPECIAL_WORKSPACE_ID = -99;
            PWORKSPACE                     = g_pCompositor->getWorkspaceByID(SPECIAL_WORKSPACE_ID);
            if (!PWORKSPACE)
                PWORKSPACE = g_pCompositor->createNewWorkspace(SPECIAL_WORKSPACE_ID, PMONITOR->m_id, "special");
        } else {
            PWORKSPACE = g_pCompositor->getWorkspaceByString(arg);
            if (!PWORKSPACE) {
                try {
                    const int workspaceID = std::stoi(arg);
                    PWORKSPACE            = g_pCompositor->getWorkspaceByID(workspaceID);
                    if (!PWORKSPACE)
                        PWORKSPACE = g_pCompositor->createNewWorkspace(workspaceID, PMONITOR->m_id);
                } catch (const std::invalid_argument&) { PWORKSPACE = g_pCompositor->createNewWorkspace(0, PMONITOR->m_id, arg); } catch (const std::out_of_range&) {
                    return {};
                }
            }
        }
        if (!PWORKSPACE)
            return {};

        if (PWORKSPACE == WDATA->window->m_workspace)
            return {};

        auto targetWorkspaceData = dataFor(PWORKSPACE);
        if (!targetWorkspaceData) {
            targetWorkspaceData       = m_workspaceDatas.emplace_back(makeShared<SWorkspaceData>(PWORKSPACE, this));
            targetWorkspaceData->self = targetWorkspaceData;
        }

        const auto NEW_COL = targetWorkspaceData->add();

        NEW_COL->columnWidth = CURRENT_COL->columnWidth;
        NEW_COL->windowDatas = CURRENT_COL->windowDatas;

        for (const auto& wd : NEW_COL->windowDatas) {
            wd->column = NEW_COL;
        }

        std::vector<PHLWINDOW> windowsToMove;
        for (const auto& wd : CURRENT_COL->windowDatas) {
            windowsToMove.push_back(wd->window.lock());
        }

        CURRENT_COL->windowDatas.clear();
        SOURCE_WS_DATA->remove(CURRENT_COL);

        CScopeGuard sg([this]() {
            m_columnMoveState.isMovingColumn    = false;
            m_columnMoveState.targetWorkspaceID = -1;

            for (auto& ws : m_workspaceDatas) {
                ws->recalculate();
            }
        });

        m_columnMoveState.isMovingColumn    = true;
        m_columnMoveState.targetWorkspaceID = PWORKSPACE->m_id;

        for (const auto& win : windowsToMove) {
            g_pCompositor->moveWindowToWorkspaceSafe(win, PWORKSPACE);
        }

        g_pCompositor->focusWindow(windowsToMove.front());
        g_pCompositor->warpCursorTo(windowsToMove.front()->middle());
    } else if (ARGS[0] == "togglefit") {
        static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:focus_fit_method");
        auto&             fitMethod  = *PFITMETHOD.ptr();
        const int         toggled    = fitMethod ^ 1;

        fitMethod = toggled;

        const auto        focusedData = dataFor(g_pCompositor->m_lastWindow.lock());
        static const auto PFSONONE    = CConfigValue<Hyprlang::INT>("plugin:hyprscrolling:fullscreen_on_one_column");

        for (const auto& ws : m_workspaceDatas) {
            if (!ws || ws->columns.empty())
                continue;

            const auto monitor = ws->workspace->m_monitor.lock();
            if (!monitor)
                continue;

            const auto USABLE = usableAreaFor(monitor);

            const auto focusedColumn = (focusedData && focusedData->column && focusedData->column->workspace.lock() == ws) ? focusedData->column.lock() : nullptr;

            const auto fallbackColumn = ws->atCenter();

            if (toggled == 1) {
                const auto columnToFit = focusedColumn ? focusedColumn : fallbackColumn;
                if (!columnToFit)
                    continue;

                double currentLeft = 0.0;
                for (const auto& col : ws->columns) {
                    const double itemWidth = *PFSONONE && ws->columns.size() == 1 ? USABLE.w : USABLE.w * col->columnWidth;

                    if (col == columnToFit) {
                        const double colLeft   = currentLeft;
                        const double colRight  = currentLeft + itemWidth;
                        const double scrollMax = std::max(ws->maxWidth() - USABLE.w, 0.0);
                        double       desiredOffset;

                        if (col == ws->columns.front())
                            desiredOffset = 0.0;
                        else
                            desiredOffset = std::clamp(colRight - USABLE.w, 0.0, scrollMax);

                        ws->leftOffset = desiredOffset;
                        break;
                    }

                    currentLeft += itemWidth;
                }
            } else {
                const auto columnToCenter = focusedColumn ? focusedColumn : fallbackColumn;
                if (!columnToCenter)
                    continue;

                ws->centerCol(columnToCenter);
            }

            ws->recalculate();
        }
    }
    return {};
}

SWindowRenderLayoutHints CScrollingLayout::requestRenderHints(PHLWINDOW a) {
    return {};
}

void CScrollingLayout::switchWindows(PHLWINDOW a, PHLWINDOW b) {
    const auto DATA1 = dataFor(a);
    const auto DATA2 = dataFor(b);

    std::swap(DATA1->window, DATA2->window);

    const auto WS1 = DATA1->column->workspace.lock();
    const auto WS2 = DATA2->column->workspace.lock();

    WS1->recalculate();

    if (WS1 == WS2)
        return;

    WS2->recalculate();
}

void CScrollingLayout::moveWindowTo(PHLWINDOW w, const std::string& dir, bool silent) {
    const auto DATA = dataFor(w);

    if (!DATA)
        return;

    const auto WS = DATA->column->workspace.lock();

    if (dir == "l") {
        const auto COL = WS->prev(DATA->column.lock());

        DATA->column->remove(w);

        if (!COL) {
            const auto NEWCOL = WS->add(-1);
            NEWCOL->add(DATA);
            WS->centerOrFitCol(NEWCOL);
        } else {
            if (COL->windowDatas.size() > 1 || DATA->column)
                COL->add(DATA, COL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y) - 1);
            else
                COL->add(DATA);
            WS->centerOrFitCol(COL);
        }
    } else if (dir == "r") {
        const auto COL = WS->next(DATA->column.lock());

        DATA->column->remove(w);

        if (!COL) {
            // make a new one
            const auto NEWCOL = WS->add();
            NEWCOL->add(DATA);
            WS->centerOrFitCol(NEWCOL);
        } else {
            if (COL->windowDatas.size() > 1 || DATA->column)
                COL->add(DATA, COL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y) - 1);
            else
                COL->add(DATA);
            WS->centerOrFitCol(COL);
        }

    } else if (dir == "t" || dir == "u")
        DATA->column->up(DATA);
    else if (dir == "b" || dir == "d")
        DATA->column->down(DATA);

    WS->recalculate();
    focusWindowUpdate(w);
    g_pCompositor->warpCursorTo(w->middle());
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
