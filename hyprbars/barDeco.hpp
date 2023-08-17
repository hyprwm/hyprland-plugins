#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include "globals.hpp"

class CHyprBar : public IHyprWindowDecoration {
  public:
    CHyprBar(CWindow*);
    virtual ~CHyprBar();

    virtual SWindowDecorationExtents getWindowDecorationExtents();

    virtual void                     draw(CMonitor*, float a, const Vector2D& offset);

    virtual eDecorationType          getDecorationType();

    virtual void                     updateWindow(CWindow*);

    virtual void                     damageEntire();

    virtual SWindowDecorationExtents getWindowDecorationReservedArea();

    virtual bool                     allowsInput();

  private:
    SWindowDecorationExtents m_seExtents;

    CWindow*                 m_pWindow = nullptr;

    Vector2D                 m_vLastWindowPos;
    Vector2D                 m_vLastWindowSize;

    CTexture                 m_tTextTex;
    CTexture                 m_tButtonsTex;

    bool                     m_bWindowSizeChanged = false;

    Vector2D                 cursorRelativeToBar();

    void                     renderBarTitle(const Vector2D& bufferSize, const float scale);
    void                     renderBarButtons(const Vector2D& bufferSize, const float scale);
    void                     onMouseDown(wlr_pointer_button_event* e);
    void                     onMouseMove(Vector2D coords);

    HOOK_CALLBACK_FN*        m_pMouseButtonCallback;
    HOOK_CALLBACK_FN*        m_pMouseMoveCallback;

    std::string              m_szLastTitle;

    bool                     m_bDraggingThis = false;
    bool                     m_bDragPending  = false;
};
