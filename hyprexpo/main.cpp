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
    if (!g_pOverview || renderingOverview || g_pOverview->blockOverviewRendering || g_pOverview->pMonitor != pMonitor)
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

//
static void swipeBegin(void* self, SCallbackInfo& info, std::any param) {
    static auto* const* PENABLE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:enable_gesture")->getDataStaticPtr();

    if (g_pOverview) {
        info.cancelled = true;
        return;
    }

    auto e = std::any_cast<wlr_pointer_swipe_begin_event*>(param);

    if (!**PENABLE || e->fingers != 4)
        return;

    info.cancelled = true;

    renderingOverview = true;
    g_pOverview       = std::make_unique<COverview>(g_pCompositor->m_pLastMonitor->activeWorkspace, true);
    renderingOverview = false;

    gestured = 0;
}

static void swipeUpdate(void* self, SCallbackInfo& info, std::any param) {
    if (!g_pOverview)
        return;

    static auto* const* PPOSITIVE = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:gesture_positive")->getDataStaticPtr();

    info.cancelled = true;

    auto e = std::any_cast<wlr_pointer_swipe_update_event*>(param);

    gestured += (**PPOSITIVE ? 1.0 : -1.0) * e->dy;

    g_pOverview->onSwipeUpdate(gestured);
}

static void swipeEnd(void* self, SCallbackInfo& info, std::any param) {
    if (!g_pOverview)
        return;

    info.cancelled = true;

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

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success)
        throw std::runtime_error("[he] Failed initializing hooks");

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", [](void* self, SCallbackInfo& info, std::any param) {
        if (!g_pOverview)
            return;
        g_pOverview->onPreRender();
    });

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeBegin", [](void* self, SCallbackInfo& info, std::any data) { swipeBegin(self, info, data); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeEnd", [](void* self, SCallbackInfo& info, std::any data) { swipeEnd(self, info, data); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "swipeUpdate", [](void* self, SCallbackInfo& info, std::any data) { swipeUpdate(self, info, data); });

    HyprlandAPI::addDispatcher(PHANDLE, "hyprexpo:expo", onExpoDispatcher);

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:columns", Hyprlang::INT{3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gap_size", Hyprlang::INT{5});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:bg_col", Hyprlang::INT{0xFF111111});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method", Hyprlang::STRING{"center current"});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:enable_gesture", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance", Hyprlang::INT{200});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gesture_positive", Hyprlang::INT{1});

    HyprlandAPI::reloadConfig();

    return {"hyprexpo", "A plugin for an overview", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
