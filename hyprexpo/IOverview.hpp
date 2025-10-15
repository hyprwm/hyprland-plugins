#pragma once
#include <hyprland/src/helpers/memory/Memory.hpp>

class IOverview {
  public:
    IOverview()          = default;
    virtual ~IOverview() = default;

    virtual void  render()           = 0;
    virtual void  damage()           = 0;
    virtual void  onDamageReported() = 0;
    virtual void  onPreRender()      = 0;

    virtual void  setClosing(bool closing) = 0;

    virtual void  resetSwipe()                = 0;
    virtual void  onSwipeUpdate(double delta) = 0;
    virtual void  onSwipeEnd()                = 0;

    virtual void  close()                  = 0;
    virtual void  selectHoveredWorkspace() = 0;

    virtual void  fullRender() = 0;

    // Keyboard navigation interface (grid-specific, may be no-op in scroll)
    virtual void  onKbMoveFocus(const std::string& dir) = 0;
    virtual void  onKbConfirm()                         = 0;
    virtual void  onKbSelectNumber(int num)             = 0;
    virtual void  onKbSelectToken(int visibleIdx)       = 0;

    bool          blockOverviewRendering = false;
    bool          blockDamageReporting   = false;

    PHLMONITORREF pMonitor;
    bool          m_isSwiping = false;
};

inline SP<IOverview> g_pOverview;
