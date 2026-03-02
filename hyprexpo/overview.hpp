#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <chrono>
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

    void setClosing(bool closing);

    void resetSwipe();
    void onSwipeUpdate(double delta);
    void onSwipeEnd();

    // close without a selection
    void          close();
    void          selectHoveredWorkspace();

    bool          blockOverviewRendering = false;
    bool          blockDamageReporting   = false;

    PHLMONITORREF pMonitor;
    bool          m_isSwiping = false;

  private:
    void       redrawID(int id, bool forcelowres = false);
    void       redrawAll(bool forcelowres = false);
    void       onWorkspaceChange();
    void       fullRender();
    void       renderLabel(SP<CTexture>& tex, const std::string& text, int size);
    Vector2D   tilePosForID(int id, Vector2D totalSize, double gapSize) const;

    int        SIDE_LENGTH           = 3;  // columns in grid
    int        GAP_WIDTH             = 5;
    CHyprColor BG_COLOR              = CHyprColor{0.1, 0.1, 0.1, 1.0};
    int        gridRows              = 3;  // rows in dynamic grid
    bool       dynamicGrid           = false;

    bool       damageDirty = false;

    struct SWorkspaceImage {
        CFramebuffer fb;
        SP<CTexture> labelTex;
        int64_t      workspaceID = -1;
        PHLWORKSPACE pWorkspace;
        CBox         box;
    };

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;

    std::vector<SWorkspaceImage> images;

    PHLWORKSPACE                 startedOn;

    PHLANIMVAR<Vector2D>         size;
    PHLANIMVAR<Vector2D>         pos;

    bool                         closing = false;

    CHyprSignalListener          mouseMoveHook;
    CHyprSignalListener          mouseButtonHook;
    CHyprSignalListener          touchMoveHook;
    CHyprSignalListener          touchDownHook;

    bool                         swipe             = false;
    bool                         swipeWasCommenced = false;

    int                          hoveredID         = -1;
    int                          selectedID        = -1;
    CHyprSignalListener          keyPressHook;
    bool                         dragging          = false;
    int                          dragSourceID      = -1;
    Vector2D                     dragStartPos;

    std::chrono::steady_clock::time_point createdAt;

    friend class COverviewPassElement;
};

inline std::unique_ptr<COverview> g_pOverview;
