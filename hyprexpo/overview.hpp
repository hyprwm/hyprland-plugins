#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>
#include <pango/pangocairo.h>

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
    void       renderText(SP<CTexture> out, const std::string& text, const CHyprColor& color, const Vector2D& bufferSize, const float scale, const int fontSize);
    double     calculateTextWidth(const std::string& text, const float scale, const int fontSize);

    int        SIDE_LENGTH = 3;
    int        GAP_WIDTH   = 5;
    CHyprColor BG_COLOR    = CHyprColor{0.1, 0.1, 0.1, 1.0};
    bool       show_label  = false;
    int        fontSize    = 24;

    enum class LabelAnchor {
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };
    LabelAnchor labelAnchor = LabelAnchor::TOP_LEFT;

    bool       damageDirty = false;

    struct SWorkspaceImage {
        CFramebuffer fb;
        int64_t      workspaceID = -1;
        PHLWORKSPACE pWorkspace;
        CBox         box;
        SP<CTexture> textTex;
    };

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;

    std::vector<SWorkspaceImage> images;

    PHLWORKSPACE                 startedOn;

    PHLANIMVAR<Vector2D>         size;
    PHLANIMVAR<Vector2D>         pos;

    bool                         closing = false;

    SP<HOOK_CALLBACK_FN>         mouseMoveHook;
    SP<HOOK_CALLBACK_FN>         mouseButtonHook;
    SP<HOOK_CALLBACK_FN>         touchMoveHook;
    SP<HOOK_CALLBACK_FN>         touchDownHook;

    bool                         swipe             = false;
    bool                         swipeWasCommenced = false;

    friend class COverviewPassElement;
};

inline std::unique_ptr<COverview> g_pOverview;
