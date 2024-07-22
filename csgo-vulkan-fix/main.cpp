#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#include <hyprland/src/managers/SeatManager.hpp>

#include "globals.hpp"

// Methods
inline CFunctionHook* g_pMouseMotionHook     = nullptr;
inline CFunctionHook* g_pSurfaceSizeHook     = nullptr;
inline CFunctionHook* g_pWLSurfaceDamageHook = nullptr;
typedef void (*origMotion)(CSeatManager*, uint32_t, const Vector2D&);
typedef void (*origSurfaceSize)(CXWaylandSurface*, const CBox&);
typedef CRegion (*origWLSurfaceDamage)(CWLSurface*);

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void hkNotifyMotion(CSeatManager* thisptr, uint32_t time_msec, const Vector2D& local) {
    static auto* const RESX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->getDataStaticPtr();
    static auto* const RESY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->getDataStaticPtr();
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    Vector2D           newCoords = local;

    if (!g_pCompositor->m_pLastWindow.expired() && g_pCompositor->m_pLastWindow.lock()->m_szInitialClass == *PCLASS && g_pCompositor->m_pLastMonitor) {
        // fix the coords
        newCoords.x *= (**RESX / g_pCompositor->m_pLastMonitor->vecSize.x) / g_pCompositor->m_pLastWindow.lock()->m_fX11SurfaceScaledBy;
        newCoords.y *= (**RESY / g_pCompositor->m_pLastMonitor->vecSize.y) / g_pCompositor->m_pLastWindow.lock()->m_fX11SurfaceScaledBy;
    }

    (*(origMotion)g_pMouseMotionHook->m_pOriginal)(thisptr, time_msec, newCoords);
}

void hkSetWindowSize(CXWaylandSurface* surface, const CBox& box) {
    static auto* const RESX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->getDataStaticPtr();
    static auto* const RESY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->getDataStaticPtr();
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

    if (!surface) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, box);
        return;
    }

    const auto SURF    = surface->surface.lock();
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(SURF);

    CBox       newBox = box;

    if (PWINDOW && PWINDOW->m_szInitialClass == *PCLASS) {
        newBox.w = **RESX;
        newBox.h = **RESY;

        CWLSurface::fromResource(SURF)->m_bFillIgnoreSmall = true;
    }

    (*(origSurfaceSize)g_pSurfaceSizeHook->m_pOriginal)(surface, newBox);
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

    auto FNS     = HyprlandAPI::findFunctionsByName(PHANDLE, "sendPointerMotion");
    bool success = !FNS.empty();
    if (success)
        g_pMouseMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)::hkNotifyMotion);
    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "configure");
    for (auto& fn : FNS) {
        if (!fn.signature.contains("XWaylandSurface"))
            continue;

        g_pSurfaceSizeHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)::hkSetWindowSize);
    }

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "computeDamage");
    for (auto& r : FNS) {
        if (!r.demangled.contains("CWLSurface"))
            continue;

        g_pWLSurfaceDamageHook = HyprlandAPI::createFunctionHook(PHANDLE, r.address, (void*)::hkWLSurfaceDamage);
        break;
    }

    success = success && g_pSurfaceSizeHook && g_pWLSurfaceDamageHook && g_pMouseMotionHook;
    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization: Failed to find required hook fns", CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[vkfix] Hooks fn init failed");
    }

    success = success && g_pWLSurfaceDamageHook->hook();
    success = success && g_pMouseMotionHook->hook();
    success = success && g_pSurfaceSizeHook->hook();

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
