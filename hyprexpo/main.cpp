#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>

#include "globals.hpp"
#include "overview.hpp"

// Methods
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
inline CFunctionHook* g_pSwipeBeginHook      = nullptr;
inline CFunctionHook* g_pSwipeEndHook        = nullptr;
inline CFunctionHook* g_pSwipeUpdateHook     = nullptr;
typedef void (*origRenderWorkspace)(void*, CMonitor*, PHLWORKSPACE, timespec*, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox*);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);
typedef void (*origSwipeBegin)(void*, wlr_pointer_swipe_begin_event*);
typedef void (*origSwipeEnd)(void*, wlr_pointer_swipe_end_event*);
typedef void (*origSwipeUpdate)(void*, wlr_pointer_swipe_update_event*);

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static bool renderingOverview = false;

//
static void hkRenderWorkspace(void* thisptr, CMonitor* pMonitor, PHLWORKSPACE pWorkspace, timespec* now, const CBox& geometry) {
    if (!g_pOverview || renderingOverview || g_pOverview->blockOverviewRendering)
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_pOriginal))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        g_pOverview->render();
}

static void hkAddDamageA(void* thisptr, const CBox* box) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR || g_pOverview->blockDamageReporting) {
        ((origAddDamageA)g_pAddDamageHookA->m_pOriginal)(thisptr, box);
        return;
    }

    g_pOverview->onDamageReported();
}

static void hkAddDamageB(void* thisptr, const pixman_region32_t* rg) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR || g_pOverview->blockDamageReporting) {
        ((origAddDamageB)g_pAddDamageHookB->m_pOriginal)(thisptr, rg);
        return;
    }

    g_pOverview->onDamageReported();
}

static float gestured = 0;

static void  hkSwipeBegin(void* thisptr, wlr_pointer_swipe_begin_event* e) {
    static auto* const* PENABLE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:enable_gesture")->getDataStaticPtr();

    if (g_pOverview)
        return;

    if (!**PENABLE || e->fingers != 4) {
        ((origSwipeBegin)g_pSwipeBeginHook->m_pOriginal)(thisptr, e);
        return;
    }

    renderingOverview = true;
    g_pOverview       = std::make_unique<COverview>(g_pCompositor->m_pLastMonitor->activeWorkspace, true);
    renderingOverview = false;

    gestured = 0;
}

static void hkSwipeUpdate(void* thisptr, wlr_pointer_swipe_update_event* e) {
    if (!g_pOverview) {
        ((origSwipeUpdate)g_pSwipeUpdateHook->m_pOriginal)(thisptr, e);
        return;
    }

    gestured += e->dy;

    g_pOverview->onSwipeUpdate(gestured);
}

static void hkSwipeEnd(void* thisptr, wlr_pointer_swipe_end_event* e) {
    if (!g_pOverview) {
        ((origSwipeEnd)g_pSwipeEndHook->m_pOriginal)(thisptr, e);
        return;
    }

    g_pOverview->onSwipeEnd();
}

static void onExpoDispatcher(std::string arg) {

    if (arg == "toggle") {
        if (g_pOverview)
            g_pOverview->close();
        else {
            renderingOverview = true;
            g_pOverview       = std::make_unique<COverview>(g_pCompositor->m_pLastMonitor->activeWorkspace);
            renderingOverview = false;
        }
        return;
    }

    if (arg == "off" || arg == "close" || arg == "disable") {
        if (g_pOverview)
            g_pOverview->close();
        return;
    }

    if (g_pOverview)
        return;

    renderingOverview = true;
    g_pOverview       = std::make_unique<COverview>(g_pCompositor->m_pLastMonitor->activeWorkspace);
    renderingOverview = false;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[he] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook renderWorkspace");

    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkRenderWorkspace);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook addDamageEPK15pixman_region32");

    g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageB);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK4CBox");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook addDamageEPK4CBox");

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "onSwipeBegin");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook onSwipeBegin");

    g_pSwipeBeginHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkSwipeBegin);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "onSwipeEnd");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook onSwipeEnd");

    g_pSwipeEndHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkSwipeEnd);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "onSwipeUpdate");
    if (FNS.empty())
        throw std::runtime_error("[he] No fns for hook onSwipeUpdate");

    g_pSwipeUpdateHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkSwipeUpdate);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();
    // mega buggy, I'll have to fix it one day.
    // success      = success && g_pSwipeBeginHook->hook();
    // success      = success && g_pSwipeEndHook->hook();
    // success      = success && g_pSwipeUpdateHook->hook();

    if (!success)
        throw std::runtime_error("[he] Failed initializing hooks");

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", [](void* self, SCallbackInfo& info, std::any param) {
        if (!g_pOverview)
            return;
        g_pOverview->onPreRender();
    });

    HyprlandAPI::addDispatcher(PHANDLE, "hyprexpo:expo", onExpoDispatcher);

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:columns", Hyprlang::INT{3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gap_size", Hyprlang::INT{5});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:bg_col", Hyprlang::INT{0xFF111111});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method", Hyprlang::STRING{"center current"});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:enable_gesture", Hyprlang::INT{1});

    HyprlandAPI::reloadConfig();

    return {"hyprexpo", "A plugin for an overview", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
