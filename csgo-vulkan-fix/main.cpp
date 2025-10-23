#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "globals.hpp"

#include <hyprutils/string/ConstVarList.hpp>
using namespace Hyprutils::String;

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

struct SAppConfig {
    std::string szClass;
    Vector2D    res;
};

std::vector<SAppConfig>  g_appConfigs;

static const SAppConfig* getAppConfig(const std::string& appClass) {
    for (const auto& ac : g_appConfigs) {
        if (ac.szClass != appClass)
            continue;
        return &ac;
    }
    return nullptr;
}

void hkNotifyMotion(CSeatManager* thisptr, uint32_t time_msec, const Vector2D& local) {
    static auto* const PFIX = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:fix_mouse")->getDataStaticPtr();

    Vector2D           newCoords = local;

    const auto         CONFIG = g_pCompositor->m_lastWindow && g_pCompositor->m_lastMonitor ? getAppConfig(g_pCompositor->m_lastWindow->m_initialClass) : nullptr;

    if (**PFIX && CONFIG) {
        // fix the coords
        newCoords.x *= (CONFIG->res.x / g_pCompositor->m_lastMonitor->m_size.x) / g_pCompositor->m_lastWindow->m_X11SurfaceScaledBy;
        newCoords.y *= (CONFIG->res.y / g_pCompositor->m_lastMonitor->m_size.y) / g_pCompositor->m_lastWindow->m_X11SurfaceScaledBy;
    }

    (*(origMotion)g_pMouseMotionHook->m_original)(thisptr, time_msec, newCoords);
}

void hkSetWindowSize(CXWaylandSurface* surface, const CBox& box) {
    if (!surface) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, box);
        return;
    }

    const auto SURF    = surface->m_surface.lock();
    const auto PWINDOW = g_pCompositor->getWindowFromSurface(SURF);

    CBox       newBox = box;

    if (!PWINDOW) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, newBox);
        return;
    }

    if (const auto CONFIG = getAppConfig(PWINDOW->m_initialClass); CONFIG) {
        newBox.w = CONFIG->res.x;
        newBox.h = CONFIG->res.y;

        CWLSurface::fromResource(SURF)->m_fillIgnoreSmall = true;
    }

    (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, newBox);
}

CRegion hkWLSurfaceDamage(CWLSurface* thisptr) {
    const auto RG = (*(origWLSurfaceDamage)g_pWLSurfaceDamageHook->m_original)(thisptr);

    if (thisptr->exists() && thisptr->getWindow()) {
        const auto CONFIG = getAppConfig(thisptr->getWindow()->m_initialClass);

        if (CONFIG) {
            const auto PMONITOR = thisptr->getWindow()->m_monitor.lock();
            if (PMONITOR)
                g_pHyprRenderer->damageMonitor(PMONITOR);
            else
                g_pHyprRenderer->damageWindow(thisptr->getWindow());
        }
    }

    return RG;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[vkfix] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w", Hyprlang::INT{1680});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h", Hyprlang::INT{1050});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:fix_mouse", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class", Hyprlang::STRING{"cs2"});

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) {
        g_appConfigs.clear();

        static auto* const RESX   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_w")->getDataStaticPtr();
        static auto* const RESY   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:res_h")->getDataStaticPtr();
        static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:csgo-vulkan-fix:class")->getDataStaticPtr();

        g_appConfigs.emplace_back(SAppConfig{.szClass = *PCLASS, .res = Vector2D{(int)**RESX, (int)**RESY}});
    });

    HyprlandAPI::addConfigKeyword(
        PHANDLE, "vkfix-app",
        [](const char* l, const char* r) -> Hyprlang::CParseResult {
            const std::string      str = r;
            CConstVarList          data(str, 0, ',', true);

            Hyprlang::CParseResult result;

            if (data.size() != 3) {
                result.setError("vkfix-app requires 3 params");
                return result;
            }

            try {
                SAppConfig config;
                config.szClass = data[0];
                config.res     = Vector2D{std::stoi(std::string{data[1]}), std::stoi(std::string{data[2]})};
                g_appConfigs.emplace_back(std::move(config));
            } catch (std::exception& e) {
                result.setError("failed to parse line");
                return result;
            }

            return result;
        },
        Hyprlang::SHandlerOptions{});

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "sendPointerMotion");
    for (auto& fn : FNS) {
        if (!fn.demangled.contains("CSeatManager"))
            continue;

        g_pMouseMotionHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)::hkNotifyMotion);
        break;
    }

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "configure");
    for (auto& fn : FNS) {
        if (!fn.demangled.contains("XWaylandSurface"))
            continue;

        g_pSurfaceSizeHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)::hkSetWindowSize);
        break;
    }

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "computeDamage");
    for (auto& r : FNS) {
        if (!r.demangled.contains("CWLSurface"))
            continue;

        g_pWLSurfaceDamageHook = HyprlandAPI::createFunctionHook(PHANDLE, r.address, (void*)::hkWLSurfaceDamage);
        break;
    }

    bool success = g_pSurfaceSizeHook && g_pWLSurfaceDamageHook && g_pMouseMotionHook;
    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization: Failed to find required hook fns", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[vkfix] Hooks fn init failed");
    }

    success = success && g_pWLSurfaceDamageHook->hook();
    success = success && g_pMouseMotionHook->hook();
    success = success && g_pSurfaceSizeHook->hook();

    if (success)
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Initialized successfully! (Anything version)", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    else {
        HyprlandAPI::addNotification(PHANDLE, "[csgo-vulkan-fix] Failure in initialization (hook failed)!", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[csgo-vk-fix] Hooks failed");
    }

    return {"csgo-vulkan-fix", "A plugin to force specific apps to a fake resolution", "Vaxry", "1.2"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
