#pragma once

#define WLR_USE_UNSTABLE

#include <src/render/decorations/IHyprWindowDecoration.hpp>
#include <src/render/OpenGL.hpp>
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

  private:
    SWindowDecorationExtents m_seExtents;

    CWindow*                 m_pWindow = nullptr;

    Vector2D                 m_vLastWindowPos;
    Vector2D                 m_vLastWindowSize;

    CTexture                 m_tTextTex;
    CTexture                 m_tButtonsTex;

    bool                     m_bWindowSizeChanged = false;

    Vector2D                 cursorRelativeToBar();

    void                     renderBarTitle(const Vector2D& bufferSize);
    void                     renderBarButtons(const Vector2D& bufferSize);
    void                     onMouseDown(wlr_pointer_button_event* e);

    HOOK_CALLBACK_FN*        m_pMouseButtonCallback;

    std::string              m_szLastTitle;
};