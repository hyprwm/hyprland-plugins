#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "globals.hpp"

// Methods
inline CFunctionHook* g_pMouseMotionHook     = nullptr;
inline CFunctionHook* g_pSurfaceSizeHook     = nullptr;
inline CFunctionHook* g_pSurfaceDamageHook   = nullptr;
inline CFunctionHook* g_pWLSurfaceDamageHook = nullptr;
typedef void (*origMotion)(wlr_seat*, uint32_t, double, double);
typedef void (*origSurfaceSize)(wlr_xwayland_surface*, int16_t, int16_t, uint16_t, uint16_t);
typedef void (*origSurfaceDamage)(wlr_surface*, pixman_region32_t*);
typedef CRegion (*origWLSurfaceDamage)(CWLSurface*);

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkNotifyMotion(wlr_seat* wlr_seat, uint32_t time_msec, double sx, double sy) {
    static auto* const RESX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->getDataStaticPtr();
    static auto* const RESY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->getDataStaticPtr();
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    if (!g_pCompositor->m_pLastWindow.expired() && g_pCompositor->m_pLastWindow.lock()->m_szInitialClass == *PCLASS && g_pCompositor->m_pLastMonitor) {
        // fix the coords
        sx *= (**RESX / g_pCompositor->m_pLastMonitor->vecSize.x) / g_pCompositor->m_pLastWindow.lock()->m_fX11SurfaceScaledBy;
        sy *= (**RESY / g_pCompositor->m_pLastMonitor->vecSize.y) / g_pCompositor->m_pLastWindow.lock()->m_fX11SurfaceScaledBy;
    }

    (*(origMotion)g_pMouseMotionHook->m_pOriginal)(wlr_seat, time_msec, sx, sy);
}

void hkSetWindowSize(wlr_xwayland_surface* surface, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    static auto* const RESX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->getDataStaticPtr();
    static auto* const RESY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->getDataStaticPtr();
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    if (!surface) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, x, y, width, height);
        return;
    }

    const auto SURF    = surface->surface;
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(SURF);

    if (PWINDOW && PWINDOW->m_szInitialClass == *PCLASS) {
        width  = **RESX;
        height = **RESY;

        CWLSurface::surfaceFromWlr(SURF)->m_bFillIgnoreSmall = true;
    }

    (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, x, y, width, height);
}

void hkSurfaceDamage(wlr_surface* surface, pixman_region32_t* damage) {
    (*(origSurfaceDamage)g_pSurfaceDamageHook->m_pOriginal)(surface, damage);

    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    const auto         SURF = CWLSurface::surfaceFromWlr(surface);

    if (SURF && SURF->exists() && SURF->getWindow() && SURF->getWindow()->m_szInitialClass == *PCLASS) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(SURF->getWindow()->m_iMonitorID);
        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
        else
            g_pHyprRenderer->damageWindow(SURF->getWindow());
    }
}

CRegion hkWLSurfaceDamage(CWLSurface* thisptr) {
    const auto         RG = (*(origWLSurfaceDamage)g_pWLSurfaceDamageHook->m_pOriginal)(thisptr);

    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    if (thisptr->exists() && thisptr->getWindow() && thisptr->getWindow()->m_szInitialClass == *PCLASS) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(thisptr->getWindow()->m_iMonitorID);
        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
        else
            g_pHyprRenderer->damageWindow(thisptr->getWindow());
    }

    return RG;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[vkfix] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w", Hyprlang::INT{1680});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h", Hyprlang::INT{1050});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class", Hyprlang::STRING{"cs2"});

    auto FNS     = HyprlandAPI::findFunctionsByName(PHANDLE, "wlr_seat_pointer_notify_motion");
    bool success = !FNS.empty();
    if (success)
        g_pMouseMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)::hkNotifyMotion);
    FNS     = HyprlandAPI::findFunctionsByName(PHANDLE, "wlr_xwayland_surface_configure");
    success = success && !FNS.empty();
    if (success)
        g_pSurfaceSizeHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)::hkSetWindowSize);
    FNS     = HyprlandAPI::findFunctionsByName(PHANDLE, "wlr_surface_get_effective_damage");
    success = success && !FNS.empty();
    if (success)
        g_pSurfaceDamageHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)::hkSurfaceDamage);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "logicalDamage");
    for (auto& r : FNS) {
        if (!r.demangled.contains("CWLSurface"))
            continue;

        g_pWLSurfaceDamageHook = HyprlandAPI::createFunctionHook(PHANDLE, r.address, (void*)::hkWLSurfaceDamage);
        break;
    }

    success = success && g_pWLSurfaceDamageHook->hook();
    success = success && g_pMouseMotionHook->hook();
    success = success && g_pSurfaceSizeHook->hook();
    success = success && g_pSurfaceDamageHook->hook();

    if (success)
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Initialized successfully! (Anything version)", CColor{0.2, 1.0, 0.2, 1.0}, 5000);
    else {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization (hook failed)!", CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[csgo-vk-fix] Hooks failed");
    }

    return {"csgo-vulkan-fix", "A plugin to force specific apps to a fake resolution", "Vaxry", "1.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
