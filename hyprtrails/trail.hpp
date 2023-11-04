#pragma once

#define WLR_USE_UNSTABLE

#include <deque>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

struct box {
    float x = 0, y = 0, w = 0, h = 0;

    Vector2D middle() { return Vector2D{x + w / 2.0, y + h / 2.0}; }
};
struct point2 {
    point2(const Vector2D& v) {
        x = v.x;
        y = v.y;
    }

    point2() {
        x = 0;
        y = 0;
    }

    float x = 0, y = 0;
};

class CTrail : public IHyprWindowDecoration {
   public:
    CTrail(CWindow*);
    virtual ~CTrail();

    virtual SWindowDecorationExtents getWindowDecorationExtents();

    virtual void draw(CMonitor*, float a, const Vector2D& offset);

    virtual eDecorationType getDecorationType();

    virtual void updateWindow(CWindow*);

    virtual void damageEntire();

    virtual SWindowDecorationExtents getWindowDecorationReservedArea();

   private:
    HOOK_CALLBACK_FN* pTickCb = nullptr;
    void onTick();

    std::deque<std::pair<box, std::chrono::system_clock::time_point>> m_dLastGeoms;

    int m_iTimer = 0;

    SWindowDecorationExtents m_seExtents;

    CWindow* m_pWindow = nullptr;

    Vector2D m_vLastWindowPos;
    Vector2D m_vLastWindowSize;

    wlr_box m_bLastBox = {0};
    bool m_bNeedsDamage = false;
};