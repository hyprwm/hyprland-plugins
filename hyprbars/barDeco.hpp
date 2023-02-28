#pragma once

#define WLR_USE_UNSTABLE

#include <src/render/decorations/IHyprWindowDecoration.hpp>
#include <src/render/OpenGL.hpp>

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

    bool                     m_bWindowSizeChanged = false;

    void                     renderBarTitle(const Vector2D& bufferSize);
    std::string              m_szLastTitle;
};