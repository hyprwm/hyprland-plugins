#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

class CBordersPlusPlus : public IHyprWindowDecoration {
  public:
    CBordersPlusPlus(CWindow*);
    virtual ~CBordersPlusPlus();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(CMonitor*, float a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(CWindow*);

    virtual void                       damageEntire();

    virtual uint64_t                   getDecorationFlags();

    virtual eDecorationLayer           getDecorationLayer();

    virtual std::string                getDisplayName();

  private:
    SWindowDecorationExtents m_seExtents;

    CWindow*                 m_pWindow = nullptr;

    Vector2D                 m_vLastWindowPos;
    Vector2D                 m_vLastWindowSize;

    double                   m_fLastThickness = 0;
};