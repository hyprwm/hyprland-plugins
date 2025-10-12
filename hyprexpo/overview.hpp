#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <vector>

#include "IOverview.hpp"

// saves on resources, but is a bit broken rn with blur.
// hyprland's fault, but cba to fix.
constexpr bool ENABLE_LOWRES = false;

class CMonitor;

class COverview : public IOverview {
  public:
    COverview(PHLWORKSPACE startedOn_, bool swipe = false);
    virtual ~COverview();

    virtual void render();
    virtual void damage();
    virtual void onDamageReported();
    virtual void onPreRender();

    virtual void setClosing(bool closing);

    virtual void resetSwipe();
    virtual void onSwipeUpdate(double delta);
    virtual void onSwipeEnd();

    // close without a selection
    virtual void  close() override;
    virtual void  selectHoveredWorkspace() override;

    // keyboard navigation interface
    virtual void  onKbMoveFocus(const std::string& dir) override;
    virtual void  onKbConfirm() override;
    virtual void  onKbSelectNumber(int num) override;
    virtual void  onKbSelectToken(int visibleIdx) override;

    virtual void  fullRender() override;

  private:
    void       redrawID(int id, bool forcelowres = false);
    void       redrawAll(bool forcelowres = false);
    void       onWorkspaceChange();
    void       ensureKbFocusInitialized();
    bool       isTileValid(int id) const;
    void       moveFocus(int dx, int dy);
    int        tileForWorkspaceID(int wsid) const;
    int        tileForVisibleIndex(int vIdx) const;
    void       enterSubmapIfEnabled();
    void       resetSubmapIfNeeded();

    int        SIDE_LENGTH = 3;
    int        GAP_WIDTH   = 5;
    CHyprColor BG_COLOR    = CHyprColor{0.1, 0.1, 0.1, 1.0};

    bool       damageDirty = false;

    struct SWorkspaceImage {
        CFramebuffer fb;
        int64_t      workspaceID = -1;
        PHLWORKSPACE pWorkspace;
        CBox         box;
        // Label textures per state for customization
        SP<CTexture> labelTexDefault;
        SP<CTexture> labelTexHover;
        SP<CTexture> labelTexFocus;
        SP<CTexture> labelTexCurrent;
        Vector2D     labelSizeDefault = {0, 0};
        Vector2D     labelSizeHover   = {0, 0};
        Vector2D     labelSizeFocus   = {0, 0};
        Vector2D     labelSizeCurrent = {0, 0};
    };

    Vector2D                     lastMousePosLocal = Vector2D{};

    int                          openedID  = -1;
    int                          closeOnID = -1;
    int                          kbFocusID = -1;
    bool                         submapActive = false;

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
