#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "globals.hpp"

// Methods
inline CFunctionHook* g_pMouseMotionHook   = nullptr;
inline CFunctionHook* g_pSurfaceSizeHook   = nullptr;
inline CFunctionHook* g_pSurfaceDamageHook = nullptr;
typedef void (*origMotion)(wlr_seat*, uint32_t, double, double);
typedef void (*origSurfaceSize)(wlr_xwayland_surface*, int16_t, int16_t, uint16_t, uint16_t);
typedef void (*origSurfaceDamage)(wlr_surface*, pixman_region32_t*);

// store the surface for csgo. May be invalid, only compare against
inline wlr_surface*          pCSGOSurface   = nullptr;
inline wlr_xwayland_surface* pCSGOXWSurface = nullptr;
inline int                   csgoMonitor    = 0;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void onNewWindow(void* self, std::any data) {
    // data is guaranteed
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    if (g_pXWaylandManager->getAppIDClass(PWINDOW) == "cs2") {
        pCSGOSurface   = g_pXWaylandManager->getWindowSurface(PWINDOW);
        pCSGOXWSurface = PWINDOW->m_uSurface.xwayland;
        csgoMonitor    = PWINDOW->m_iMonitorID;
    }
}

void hkNotifyMotion(wlr_seat* wlr_seat, uint32_t time_msec, double sx, double sy) {
    static auto* const RESX = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->intValue;
    static auto* const RESY = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->intValue;

    if (g_pCompositor->m_pLastWindow && g_pCompositor->m_pLastWindow->m_szInitialClass == "cs2" && g_pCompositor->m_pLastMonitor) {
        // fix the coords
        sx *= *RESX / g_pCompositor->m_pLastMonitor->vecSize.x;
        sy *= *RESY / g_pCompositor->m_pLastMonitor->vecSize.y;
    }

    (*(origMotion)g_pMouseMotionHook->m_pOriginal)(wlr_seat, time_msec, sx, sy);
}

void hkSetWindowSize(wlr_xwayland_surface* surface, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    static auto* const RESX = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->intValue;
    static auto* const RESY = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->intValue;

    if (!surface) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, x, y, width, height);
        return;
    }

    const auto SURF    = surface->surface;
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(SURF);

    if (PWINDOW && PWINDOW->m_szInitialClass == "cs2") {
        width  = *RESX;
        height = *RESY;

        CWLSurface::surfaceFromWlr(SURF)->m_bFillIgnoreSmall = true;
    }

    (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, x, y, width, height);
}

void hkSurfaceDamage(wlr_surface* surface, pixman_region32_t* damage) {
    (*(origSurfaceDamage)g_pSurfaceDamageHook->m_pOriginal)(surface, damage);

    if (surface == pCSGOSurface) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(csgoMonitor);

        if (PMONITOR)
            pixman_region32_union_rect(damage, damage, 0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y);
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w", SConfigValue{.intValue = 1680});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h", SConfigValue{.intValue = 1050});

    g_pMouseMotionHook   = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&wlr_seat_pointer_notify_motion, (void*)&hkNotifyMotion);
    g_pSurfaceSizeHook   = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&wlr_xwayland_surface_configure, (void*)&hkSetWindowSize);
    g_pSurfaceDamageHook = HyprlandAPI::createFunctionHook(PHANDLE, (void*)&wlr_surface_get_effective_damage, (void*)&hkSurfaceDamage);
    bool hkResult        = g_pMouseMotionHook->hook();
    hkResult             = hkResult && g_pSurfaceSizeHook->hook();
    hkResult             = hkResult && g_pSurfaceDamageHook->hook();

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    if (hkResult)
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Initialized successfully! (CS2 version)", CColor{0.2, 1.0, 0.2, 1.0}, 5000);
    else {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization (hook failed)!", CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[csgo-vk-fix] Hooks failed");
    }

    return {"csgo-vulkan-fix", "A plugin to fix incorrect mouse offsets on csgo in Vulkan", "Vaxry", "1.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}