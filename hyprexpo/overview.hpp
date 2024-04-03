#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

class CMonitor;

class COverview {
  public:
    COverview(PHLWORKSPACE startedOn_, bool swipe = false);
    ~COverview();

    void render();
    void damage();
    void onDamageReported();
    void onPreRender();

    void onSwipeUpdate(double delta);
    void onSwipeEnd();

    // close without a selection
    void      close();

    bool      blockOverviewRendering = false;
    bool      blockDamageReporting   = false;

    CMonitor* pMonitor = nullptr;

  private:
    void   redrawID(int id, bool forcelowres = false);
    void   redrawAll(bool forcelowres = false);
    void   onWorkspaceChange();

    int    SIDE_LENGTH = 3;
    int    GAP_WIDTH   = 5;
    CColor BG_COLOR    = CColor{0.1, 0.1, 0.1, 1.0};

    bool   damageDirty = false;

    struct SWorkspaceImage {
        CFramebuffer fb;
        int64_t      workspaceID = -1;
        PHLWORKSPACE pWorkspace;
        CBox         box;
    };

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;

    std::vector<SWorkspaceImage> images;

    PHLWORKSPACE                 startedOn;

    CAnimatedVariable<Vector2D>  size;
    CAnimatedVariable<Vector2D>  pos;

    bool                         closing = false;

    HOOK_CALLBACK_FN*            mouseMoveHook   = nullptr;
    HOOK_CALLBACK_FN*            mouseButtonHook = nullptr;

    bool                         swipe = false;
};

inline std::unique_ptr<COverview> g_pOverview;