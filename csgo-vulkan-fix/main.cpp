#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowQuery.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/xwayland/XSurface.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>

#include "globals.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// Methods
inline CFunctionHook* g_pMouseMotionHook     = nullptr;
inline CFunctionHook* g_pSurfaceSizeHook     = nullptr;
inline CFunctionHook* g_pWLSurfaceDamageHook = nullptr;
typedef void (*origMotion)(CSeatManager*, uint32_t, const Vector2D&);
typedef void (*origSurfaceSize)(CXWaylandSurface*, const CBox&);
typedef CRegion (*origWLSurfaceDamage)(Desktop::View::CWLSurface*);

static struct {
    SP<Config::Values::CBoolValue> fixMouse;
} configValues;

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
    Vector2D           newCoords  = local;
    auto               focusState = Desktop::focusState();
    auto               window     = focusState->window();
    auto               monitor    = focusState->monitor();

    const auto         CONFIG = window && monitor ? getAppConfig(window->m_initialClass) : nullptr;

    if (configValues.fixMouse->value() && CONFIG) {
        // fix the coords
        newCoords.x *= (CONFIG->res.x / monitor->m_size.x) / window->m_X11SurfaceScaledBy;
        newCoords.y *= (CONFIG->res.y / monitor->m_size.y) / window->m_X11SurfaceScaledBy;
    }

    (*(origMotion)g_pMouseMotionHook->m_original)(thisptr, time_msec, newCoords);
}

void hkSetWindowSize(CXWaylandSurface* surface, const CBox& box) {
    if (!surface) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, box);
        return;
    }

    const auto SURF    = surface->m_surface.lock();
    const auto CWLSURF = Desktop::View::CWLSurface::fromResource(SURF);
    const auto PWINDOW = CWLSURF ? Desktop::View::CWindow::fromView(CWLSURF->view()) : nullptr;

    CBox       newBox = box;

    if (!PWINDOW) {
        (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, newBox);
        return;
    }

    if (const auto CONFIG = getAppConfig(PWINDOW->m_initialClass); CONFIG) {
        newBox.w = CONFIG->res.x;
        newBox.h = CONFIG->res.y;

        Desktop::View::CWLSurface::fromResource(SURF)->m_fillIgnoreSmall = true;
    }

    (*(origSurfaceSize)g_pSurfaceSizeHook->m_original)(surface, newBox);
}

CRegion hkWLSurfaceDamage(Desktop::View::CWLSurface* thisptr) {
    const auto RG = (*(origWLSurfaceDamage)g_pWLSurfaceDamageHook->m_original)(thisptr);

    if (thisptr->exists() && Desktop::View::CWindow::fromView(thisptr->view())) {
        const auto WINDOW = Desktop::View::CWindow::fromView(thisptr->view());
        const auto CONFIG = getAppConfig(WINDOW->m_initialClass);

        if (CONFIG) {
            const auto PMONITOR = WINDOW->m_monitor.lock();
            if (PMONITOR)
                g_pHyprRenderer->damageMonitor(PMONITOR);
            else
                g_pHyprRenderer->damageWindow(WINDOW);
        }
    }

    return RG;
}

int vkfixAppLua(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(L, "vkfix_app: expected a table { app, w, h }");

    SAppConfig config;

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "app");
        
        if (!lua_isstring(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "vkfix_app: app must be a class string");

        config.szClass = lua_tostring(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "w");
        
        if (!lua_isinteger(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "vkfix_app: w must be an integer");

        config.res.x = lua_tointeger(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "h");
        
        if (!lua_isinteger(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "vkfix_app: h must be an integer");

        config.res.y = lua_tointeger(L, -1);
    }

    g_appConfigs.emplace_back(std::move(config));

    return 0;
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

    static auto P = Event::bus()->m_events.config.preReload.listen([&] { g_appConfigs.clear(); });

    HyprlandAPI::addLuaFunction(PHANDLE, "csgo_vulkan_fix", "vkfix_app", ::vkfixAppLua);

    configValues.fixMouse =
        makeShared<Config::Values::CBoolValue>("plugin:csgo_vulkan_fix:fix_mouse", "Whether to fix the mouse position. A select few apps might be wonky with this.", true);
    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.fixMouse);

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
    configValues = {};
}
