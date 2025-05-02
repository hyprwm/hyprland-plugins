#pragma once

#include <vector>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>

class CScrollingLayout;
struct SColumnData;
struct SWorkspaceData;

struct SScrollingWindowData {
    SScrollingWindowData(PHLWINDOW w, SP<SColumnData> col) : window(w), column(col) {
        ;
    }

    PHLWINDOWREF    window;
    WP<SColumnData> column;

    CBox            layoutBox;
};

struct SColumnData {
    SColumnData(SP<SWorkspaceData> ws) : workspace(ws) {
        ;
    }

    void                                  add(PHLWINDOW w);
    void                                  add(SP<SScrollingWindowData> w);
    void                                  remove(PHLWINDOW w);

    void up(SP<SScrollingWindowData> w);
    void down(SP<SScrollingWindowData> w);

    std::vector<SP<SScrollingWindowData>> windowDatas;
    float                                 columnSize  = 1.F;
    float                                 columnWidth = 1.F;
    WP<SWorkspaceData>                    workspace;

    WP<SColumnData>                       self;
};

struct SWorkspaceData {
    SWorkspaceData(PHLWORKSPACE w, CScrollingLayout* l) : workspace(w), layout(l) {
        ;
    }

    PHLWORKSPACEREF              workspace;
    std::vector<SP<SColumnData>> columns;
    int                          leftOffset = 0;

    SP<SColumnData>              add();
    void                         remove(SP<SColumnData> c);
    double                       maxWidth();
    SP<SColumnData>              next(SP<SColumnData> c);
    SP<SColumnData>              prev(SP<SColumnData> c);
    SP<SColumnData>              atCenter();

    void                         centerCol(SP<SColumnData> c);

    void                         recalculate();

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

  private:
    std::vector<SP<SWorkspaceData>> m_workspaceDatas;

    SP<SWorkspaceData>              dataFor(PHLWORKSPACE ws);
    SP<SScrollingWindowData>        dataFor(PHLWINDOW w);
    SP<SWorkspaceData>              currentWorkspaceData();

    void                            applyNodeDataToWindow(SP<SScrollingWindowData> node, bool force);

    friend struct SWorkspaceData;
};