#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <lua.hpp>
#include <hyprutils/string/ConstVarList.hpp>
using namespace Hyprutils::String;

#include "globals.hpp"
#include "overview.hpp"
#include "ExpoGesture.hpp"

// Methods
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static bool g_unloading = false;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static bool       renderingOverview = false;

const std::string KEYWORD_EXPO_GESTURE = "hyprexpo-gesture";

//
static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry) {
    if (!g_pOverview || renderingOverview || g_pOverview->blockOverviewRendering || g_pOverview->pMonitor != pMonitor)
        ((origRenderWorkspace)(g_pRenderWorkspaceHook->m_original))(thisptr, pMonitor, pWorkspace, now, geometry);
    else
        g_pOverview->render();
}

static void hkAddDamageA(void* thisptr, const CBox& box) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR->m_self || g_pOverview->blockDamageReporting) {
        ((origAddDamageA)g_pAddDamageHookA->m_original)(thisptr, box);
        return;
    }

    g_pOverview->onDamageReported();
}

static void hkAddDamageB(void* thisptr, const pixman_region32_t* rg) {
    const auto PMONITOR = (CMonitor*)thisptr;

    if (!g_pOverview || g_pOverview->pMonitor != PMONITOR->m_self || g_pOverview->blockDamageReporting) {
        ((origAddDamageB)g_pAddDamageHookB->m_original)(thisptr, rg);
        return;
    }

    g_pOverview->onDamageReported();
}

static PHLWINDOW windowToBringFromWorkspace(const PHLWORKSPACE& workspace) {
    if (!workspace)
        return nullptr;

    for (auto it = g_pCompositor->m_windows.rbegin(); it != g_pCompositor->m_windows.rend(); ++it) {
        const auto& w = *it;
        if (!w || w->m_workspace != workspace || !w->m_isMapped || w->isHidden())
            continue;

        return w;
    }

    return nullptr;
}

static SDispatchResult bringWindowFromWorkspace(int64_t sourceWorkspaceID) {
    if (sourceWorkspaceID == WORKSPACE_INVALID)
        return {.success = false, .error = "selected workspace is empty"};

    const auto FOCUSSTATE = Desktop::focusState();
    const auto MONITOR    = FOCUSSTATE->monitor();
    if (!MONITOR || !MONITOR->m_activeWorkspace)
        return {.success = false, .error = "no active monitor/workspace"};

    if (sourceWorkspaceID == MONITOR->activeWorkspaceID())
        return {};

    const auto SOURCEWORKSPACE = g_pCompositor->getWorkspaceByID(sourceWorkspaceID);
    if (!SOURCEWORKSPACE)
        return {.success = false, .error = "selected workspace is not open"};

    const auto WINDOW = windowToBringFromWorkspace(SOURCEWORKSPACE);
    if (!WINDOW)
        return {.success = false, .error = "selected workspace has no mapped windows"};

    g_pCompositor->moveWindowToWorkspaceSafe(WINDOW, MONITOR->m_activeWorkspace);
    FOCUSSTATE->fullWindowFocus(WINDOW, Desktop::FOCUS_REASON_KEYBIND);
    g_pCompositor->warpCursorTo(WINDOW->middle());
    return {};
}

static SDispatchResult onExpoDispatcher(std::string arg) {

    if (g_pOverview && g_pOverview->m_isSwiping)
        return {.success = false, .error = "already swiping"};

    if (arg == "select") {
        if (g_pOverview) {
            g_pOverview->selectHoveredWorkspace();
            g_pOverview->close();
        }
        return {};
    }
    if (arg == "bring") {
        if (g_pOverview) {
            g_pOverview->selectHoveredWorkspace();
            const auto BRINGRESULT = bringWindowFromWorkspace(g_pOverview->selectedWorkspaceID());
            g_pOverview->close(false);
            return BRINGRESULT;
        }
        return {};
    }
    if (arg == "toggle") {
        if (g_pOverview)
            g_pOverview->close();
        else {
            renderingOverview = true;
            g_pOverview       = std::make_unique<COverview>(Desktop::focusState()->monitor()->m_activeWorkspace);
            renderingOverview = false;
        }
        return {};
    }

    if (arg == "off" || arg == "close" || arg == "disable") {
        if (g_pOverview)
            g_pOverview->close();
        return {};
    }

    if (g_pOverview)
        return {};

    renderingOverview = true;
    g_pOverview       = std::make_unique<COverview>(Desktop::focusState()->monitor()->m_activeWorkspace);
    renderingOverview = false;
    return {};
}

static int luaExpo(lua_State* L) {
    const auto RESULT = onExpoDispatcher(luaL_optstring(L, 1, "toggle"));
    if (!RESULT.success)
        return luaL_error(L, "%s", RESULT.error.c_str());
    return 0;
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

static bool addConfigValue(SP<Config::Values::IValue> value) {
    const auto RET = Config::mgr()->registerPluginValue(PHANDLE, value);
    if (!RET) {
        Log::logger->log(Log::ERR, "[hyprexpo] failed to register plugin value \"{}\": {}", value->name(), RET.error());
        return false;
    }

    return true;
}

static Hyprlang::CParseResult expoGestureKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    CConstVarList             data(RHS);

    size_t                    fingerCount = 0;
    eTrackpadGestureDirection direction   = TRACKPAD_GESTURE_DIR_NONE;

    try {
        fingerCount = std::stoul(std::string{data[0]});
    } catch (...) {
        result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
        return result;
    }

    if (fingerCount <= 1 || fingerCount >= 10) {
        result.setError(std::format("Invalid value {} for finger count", data[0]).c_str());
        return result;
    }

    direction = g_pTrackpadGestures->dirForString(data[1]);

    if (direction == TRACKPAD_GESTURE_DIR_NONE) {
        result.setError(std::format("Invalid direction: {}", data[1]).c_str());
        return result;
    }

    int      startDataIdx   = 2;
    uint32_t modMask        = 0;
    float    deltaScale     = 1.F;
    bool     disableInhibit = false;

    for (const auto arg : std::string(LHS).substr(KEYWORD_EXPO_GESTURE.size())) {
        switch (arg) {
            case 'p': disableInhibit = true; break;
            default: result.setError("hyprexpo-gesture: invalid flag"); return result;
        }
    }

    while (true) {

        if (data[startDataIdx].starts_with("mod:")) {
            modMask = g_pKeybindManager->stringToModMask(std::string{data[startDataIdx].substr(4)});
            startDataIdx++;
            continue;
        } else if (data[startDataIdx].starts_with("scale:")) {
            try {
                deltaScale = std::clamp(std::stof(std::string{data[startDataIdx].substr(6)}), 0.1F, 10.F);
                startDataIdx++;
                continue;
            } catch (...) {
                result.setError(std::format("Invalid delta scale: {}", std::string{data[startDataIdx].substr(6)}).c_str());
                return result;
            }
        }

        break;
    }

    std::expected<void, std::string> resultFromGesture;

    if (data[startDataIdx] == "expo")
        resultFromGesture = g_pTrackpadGestures->addGesture(makeUnique<CExpoGesture>(), fingerCount, direction, modMask, deltaScale, disableInhibit);
    else if (data[startDataIdx] == "unset")
        resultFromGesture = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale, disableInhibit);
    else {
        result.setError(std::format("Invalid gesture: {}", data[startDataIdx]).c_str());
        return result;
    }

    if (!resultFromGesture) {
        result.setError(resultFromGesture.error().c_str());
        return result;
    }

    return result;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        failNotif("Version mismatch (headers ver is not equal to running hyprland ver)");
        throw std::runtime_error("[he] Version mismatch");
    }

    auto FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
    if (FNS.empty()) {
        failNotif("no fns for hook renderWorkspace");
        throw std::runtime_error("[he] No fns for hook renderWorkspace");
    }

    g_pRenderWorkspaceHook = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkRenderWorkspace);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "addDamageEPK15pixman_region32");
    if (FNS.empty()) {
        failNotif("no fns for hook addDamageEPK15pixman_region32");
        throw std::runtime_error("[he] No fns for hook addDamageEPK15pixman_region32");
    }

    g_pAddDamageHookB = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageB);

    FNS = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    if (FNS.empty()) {
        failNotif("no fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
        throw std::runtime_error("[he] No fns for hook _ZN8CMonitor9addDamageERKN9Hyprutils4Math4CBoxE");
    }

    g_pAddDamageHookA = HyprlandAPI::createFunctionHook(PHANDLE, FNS[0].address, (void*)hkAddDamageA);

    bool success = g_pRenderWorkspaceHook->hook();
    success      = success && g_pAddDamageHookA->hook();
    success      = success && g_pAddDamageHookB->hook();

    if (!success) {
        failNotif("Failed initializing hooks");
        throw std::runtime_error("[he] Failed initializing hooks");
    }

    static auto P = Event::bus()->m_events.render.pre.listen([](PHLMONITOR) {
        if (!g_pOverview)
            return;
        g_pOverview->onPreRender();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:expo", ::onExpoDispatcher);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprexpo", "expo", ::luaExpo);

    HyprlandAPI::addConfigKeyword(PHANDLE, KEYWORD_EXPO_GESTURE, ::expoGestureKeyword, {true});

    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:columns", "columns", 3));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gap_size", "gap size", 5));
    addConfigValue(makeShared<Config::Values::CColorValue>("plugin:hyprexpo:bg_col", "background color", 0xFF111111));
    addConfigValue(makeShared<Config::Values::CStringValue>("plugin:hyprexpo:workspace_method", "workspace method", "center current"));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:skip_empty", "skip empty workspaces", 0));
    addConfigValue(makeShared<Config::Values::CIntValue>("plugin:hyprexpo:gesture_distance", "gesture distance", 200));

    return {"hyprexpo", "A plugin for an overview", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");

    g_unloading = true;

    Config::mgr()->reload(); // we need to reload now to clear all the gestures
}
