#pragma once

#define WLR_USE_UNSTABLE

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

class CBordersPlusPlus : public IHyprWindowDecoration {
  public:
    CBordersPlusPlus(PHLWINDOW);
    virtual ~CBordersPlusPlus();

    virtual SDecorationPositioningInfo getPositioningInfo();

    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);

    virtual void                       draw(PHLMONITOR, float const& a);

    virtual eDecorationType            getDecorationType();

    virtual void                       updateWindow(PHLWINDOW);

    virtual void                       damageEntire();

    virtual uint64_t                   getDecorationFlags();

    virtual eDecorationLayer           getDecorationLayer();

    virtual std::string                getDisplayName();

  private:
    void         drawPass(PHLMONITOR, float const& a);

    SBoxExtents  m_seExtents;

    PHLWINDOWREF m_pWindow;

    CBox         m_bLastRelativeBox;
    CBox         m_bAssignedGeometry;

    Vector2D     m_lastWindowPos;
    Vector2D     m_lastWindowSize;

    double       m_fLastThickness = 0;

    friend class CBorderPPPassElement;
};
