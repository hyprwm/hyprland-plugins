#pragma once

#include <vector>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>

class CScrollingLayout;
struct SColumnData;
struct SWorkspaceData;

struct SScrollingWindowData {
    SScrollingWindowData(PHLWINDOW w, SP<SColumnData> col, float ws = 1.F) : window(w), column(col), windowSize(ws) {
        ;
    }

    PHLWINDOWREF    window;
    WP<SColumnData> column;
    float           windowSize             = 1.F;
    bool            ignoreFullscreenChecks = false;
    PHLWORKSPACEREF overrideWorkspace;

    CBox            layoutBox;
};

struct SColumnData {
    SColumnData(SP<SWorkspaceData> ws) : workspace(ws) {
        ;
    }

    void                                  add(PHLWINDOW w);
    void                                  add(PHLWINDOW w, int after);
    void                                  add(SP<SScrollingWindowData> w);
    void                                  add(SP<SScrollingWindowData> w, int after);
    void                                  remove(PHLWINDOW w);
    bool                                  has(PHLWINDOW w);
    size_t                                idx(PHLWINDOW w);
    size_t                                idxForHeight(float y);

    void                                  up(SP<SScrollingWindowData> w);
    void                                  down(SP<SScrollingWindowData> w);

    SP<SScrollingWindowData>              next(SP<SScrollingWindowData> w);
    SP<SScrollingWindowData>              prev(SP<SScrollingWindowData> w);

    std::vector<SP<SScrollingWindowData>> windowDatas;
    float                                 columnSize  = 1.F;
    float                                 columnWidth = 1.F;
    WP<SWorkspaceData>                    workspace;
    WP<SScrollingWindowData>              lastFocusedWindow;

    WP<SColumnData>                       self;
};

struct SWorkspaceData {
    SWorkspaceData(PHLWORKSPACE w, CScrollingLayout* l) : workspace(w), layout(l) {
        ;
    }

    PHLWORKSPACEREF              workspace;
    std::vector<SP<SColumnData>> columns;
    float                        leftOffset = 0;

    SP<SColumnData>              add();
    SP<SColumnData>              add(int after);
    int64_t                      idx(SP<SColumnData> c);
    void                         remove(SP<SColumnData> c);
    double                       maxWidth();
    SP<SColumnData>              next(SP<SColumnData> c);
    SP<SColumnData>              prev(SP<SColumnData> c);
    SP<SColumnData>              atCenter();

    bool                         visible(SP<SColumnData> c);
    void                         centerCol(SP<SColumnData> c);
    void                         fitCol(SP<SColumnData> c);
    void                         centerOrFitCol(SP<SColumnData> c);

    void                         recalculate(bool forceInstant = false);

    CScrollingLayout*            layout = nullptr;
    WP<SWorkspaceData>           self;
};

class CScrollingLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const MONITORID&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     onBeginDragWindow();
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW);
    virtual void                     switchWindows(PHLWINDOW, PHLWINDOW);
    virtual void                     moveWindowTo(PHLWINDOW, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(PHLWINDOW, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to);
    virtual Vector2D                 predictSizeForNewWindowTiled();

    virtual void                     onEnable();
    virtual void                     onDisable();

    CBox                             usableAreaFor(PHLMONITOR m);

  private:
    std::vector<SP<SWorkspaceData>> m_workspaceDatas;

    SP<HOOK_CALLBACK_FN>            m_configCallback;
    SP<HOOK_CALLBACK_FN>            m_focusCallback;

    struct {
        bool isMovingColumn    = false;
        int  targetWorkspaceID = -1;
    } m_columnMoveState;

    struct {
        std::vector<float> configuredWidths;
    } m_config;

    SP<SScrollingWindowData> findBestNeighbor(SP<SScrollingWindowData> pCurrent, SP<SColumnData> pTargetCol);
    SP<SWorkspaceData>       dataFor(PHLWORKSPACE ws);
    SP<SScrollingWindowData> dataFor(PHLWINDOW w);
    SP<SWorkspaceData>       currentWorkspaceData();

    void                     applyNodeDataToWindow(SP<SScrollingWindowData> node, bool instant, bool hasWindowsRight, bool hasWindowsLeft);
    void                     focusWindowUpdate(PHLWINDOW pWindow);

    friend struct SWorkspaceData;
};
