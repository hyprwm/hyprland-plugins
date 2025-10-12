#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/trackpad/GestureTypes.hpp>
#include <hyprland/src/managers/input/trackpad/TrackpadGestures.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>

#include <hyprutils/string/ConstVarList.hpp>
using namespace Hyprutils::String;

#include <cctype>
#include <optional>

#include "globals.hpp"
#include "overview.hpp"
#include "scrollOverview.hpp"
#include "ExpoGesture.hpp"

// Methods
inline CFunctionHook* g_pRenderWorkspaceHook = nullptr;
inline CFunctionHook* g_pAddDamageHookA      = nullptr;
inline CFunctionHook* g_pAddDamageHookB      = nullptr;
typedef void (*origRenderWorkspace)(void*, PHLMONITOR, PHLWORKSPACE, timespec*, const CBox&);
typedef void (*origAddDamageA)(void*, const CBox&);
typedef void (*origAddDamageB)(void*, const pixman_region32_t*);

static bool g_unloading = false;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static bool renderingOverview = false;

// forward declarations for new dispatchers
static SDispatchResult onKbFocusDispatcher(std::string arg);
static SDispatchResult onKbConfirmDispatcher(std::string arg);
static SDispatchResult onKbSelectNumberDispatcher(std::string arg);
static SDispatchResult onKbSelectTokenDispatcher(std::string arg);
static SDispatchResult onKbSelectIndexDispatcher(std::string arg);

//
static void hkRenderWorkspace(void* thisptr, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, timespec* now, const CBox& geometry) {
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

static SDispatchResult onExpoDispatcher(std::string arg) {

    IS_SCROLLING = g_pLayoutManager->getCurrentLayout()->getLayoutName() == "scrolling";

    if (g_pOverview && g_pOverview->m_isSwiping)
        return {.success = false, .error = "already swiping"};

    if (arg == "select") {
        if (g_pOverview) {
            g_pOverview->selectHoveredWorkspace();
            g_pOverview->close();
        }
        return {};
    }
    if (arg == "toggle") {
        if (g_pOverview)
            g_pOverview->close();
        else {
            renderingOverview = true;
            if (IS_SCROLLING)
                g_pOverview = makeShared<CScrollOverview>(g_pCompositor->m_lastMonitor->m_activeWorkspace);
            else
                g_pOverview = makeShared<COverview>(g_pCompositor->m_lastMonitor->m_activeWorkspace);
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
    if (IS_SCROLLING)
        g_pOverview = makeShared<CScrollOverview>(g_pCompositor->m_lastMonitor->m_activeWorkspace);
    else
        g_pOverview = makeShared<COverview>(g_pCompositor->m_lastMonitor->m_activeWorkspace);
    renderingOverview = false;
    return {};
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[hyprexpo] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

static Hyprlang::CParseResult workspaceMethodKeyword(const char* LHS, const char* RHS) {
    Hyprlang::CParseResult result;

    if (g_unloading)
        return result;

    // Parse format: either "method workspace" (global) or "MONITOR_NAME method workspace" (per-monitor)
    // Examples:
    //   "workspace_method = center current" (global)
    //   "workspace_method = DP-1 first 19" (per-monitor)
    CConstVarList data(RHS);

    if (data.size() == 2) {
        // Global config format: method workspace
        // This is handled by the plugin config value, not here
        // Just return success and let the normal config system handle it
        return result;
    } else if (data.size() >= 3) {
        // Per-monitor format: MONITOR_NAME method workspace
        const std::string monitorName = std::string{data[0]};
        const std::string methodType = std::string{data[1]};
        const std::string workspace = std::string{data[2]};

        if (methodType != "center" && methodType != "first") {
            result.setError(std::format("Invalid method type '{}', expected 'center' or 'first'", methodType).c_str());
            return result;
        }

        // Store in global map
        g_monitorWorkspaceMethods[monitorName] = methodType + " " + workspace;
        return result;
    } else {
        result.setError("workspace_method requires format: <center|first> <workspace> OR MONITOR_NAME <center|first> <workspace>");
        return result;
    }
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

    int      startDataIdx = 2;
    uint32_t modMask      = 0;
    float    deltaScale   = 1.F;

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
        resultFromGesture = g_pTrackpadGestures->addGesture(makeUnique<CExpoGesture>(), fingerCount, direction, modMask, deltaScale);
    else if (data[startDataIdx] == "unset")
        resultFromGesture = g_pTrackpadGestures->removeGesture(fingerCount, direction, modMask, deltaScale);
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

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
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

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "preRender", [](void* self, SCallbackInfo& info, std::any param) {
        if (!g_pOverview)
            return;
        g_pOverview->onPreRender();
    });

    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:expo", ::onExpoDispatcher);

    // keyboard navigation dispatchers
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_focus", ::onKbFocusDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_confirm", ::onKbConfirmDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_selectn", ::onKbSelectNumberDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_select", ::onKbSelectTokenDispatcher);
    HyprlandAPI::addDispatcherV2(PHANDLE, "hyprexpo:kb_selecti", ::onKbSelectIndexDispatcher);

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprexpo_gesture", ::expoGestureKeyword, {});
    HyprlandAPI::addConfigKeyword(PHANDLE, "workspace_method", ::workspaceMethodKeyword, {});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:columns", Hyprlang::INT{3});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gaps_in", Hyprlang::INT{5});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:bg_col", Hyprlang::INT{0xFF111111});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:workspace_method", Hyprlang::STRING{"center current"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:skip_empty", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:scrolling:scroll_moves_up_down", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:scrolling:default_zoom", Hyprlang::FLOAT{0.5});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gesture_distance", Hyprlang::INT{200});

    // keyboard navigation + styling
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:keynav_enable", Hyprlang::INT{1});
    // Border configuration - supports both solid colors and gradients
    // Solid: rgb(rrggbb) or 0xAARRGGBB
    // Gradient: rgba(rrggbbaa) rgba(rrggbbaa) 45deg
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_width", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_color", Hyprlang::STRING{""});           // default border (unused tiles)
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_color_current", Hyprlang::STRING{"rgb(66ccff)"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_color_focus", Hyprlang::STRING{"rgb(ffcc66)"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_color_hover", Hyprlang::STRING{"rgb(aabbcc)"});
    // Deprecated but supported for backwards compatibility
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_style", Hyprlang::STRING{"simple"});     // ignored, auto-detected from format
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_enable", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_color", Hyprlang::INT{0xFFFFFFFF});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_font_size", Hyprlang::INT{16});
    // label_text_mode: token (default) | id | index
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_text_mode", Hyprlang::STRING{"token"});
    // Optional override map for up to 50 tokens, comma-separated. Empty entries allowed.
    // Example: "1,2,3,4,5,6,7,8,9,0,!,@,#,$,%,^,&,*,(,),a,..."
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_token_map", Hyprlang::STRING{""});

    // tile rounding (rounded corners for workspace previews)
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_power", Hyprlang::FLOAT{2.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_focus", Hyprlang::INT{-1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_current", Hyprlang::INT{-1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:tile_rounding_hover", Hyprlang::INT{-1});

    // (shadows moved to feature/shadows branch)
    // defaults: center/middle within the label container
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_position", Hyprlang::STRING{"center"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_x", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_offset_y", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_show", Hyprlang::STRING{"always"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_color_default", Hyprlang::INT{0xFFFFFFFF});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_color_hover", Hyprlang::INT{0xFFEEEEEE});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_color_focus", Hyprlang::INT{0xFFFFCC66});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_color_current", Hyprlang::INT{0xFF66CCFF});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_hover", Hyprlang::FLOAT{1.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_scale_focus", Hyprlang::FLOAT{1.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_enable", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_color", Hyprlang::INT{0x88000000});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_rounding", Hyprlang::INT{8});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_bg_shape", Hyprlang::STRING{"circle"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_padding", Hyprlang::INT{8});
    // label font styling and pixel snapping
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_font_family", Hyprlang::STRING{"sans"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_font_bold", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_font_italic", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_text_underline", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_text_strikethrough", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_pixel_snap", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_center_adjust_x", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:label_center_adjust_y", Hyprlang::INT{0});
    // gaps
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:gaps_out", Hyprlang::INT{0});
    // Deprecated: use border_color_* instead (supports both solid and gradient)
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_current", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_focus", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:border_grad_hover", Hyprlang::STRING{""});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_h", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:keynav_wrap_v", Hyprlang::INT{1});
    // default off: spatial moves by default
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprexpo:keynav_reading_order", Hyprlang::INT{0});

    HyprlandAPI::reloadConfig();

    return {"hyprexpo", "A plugin for an overview", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pHyprRenderer->m_renderPass.removeAllOfType("COverviewPassElement");

    g_unloading = true;

    g_pConfigManager->reload(); // we need to reload now to clear all the gestures
}

//
// New dispatchers for keyboard navigation
//

static SDispatchResult onKbFocusDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    if (arg == "left" || arg == "right" || arg == "up" || arg == "down") {
        g_pOverview->onKbMoveFocus(arg);
        return {};
    }

    return {.success = false, .error = "invalid arg. expected left|right|up|down"};
}

static SDispatchResult onKbConfirmDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    g_pOverview->onKbConfirm();
    return {};
}

static SDispatchResult onKbSelectNumberDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};

    // trim spaces
    while (!arg.empty() && std::isspace(arg.front()))
        arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back()))
        arg.pop_back();

    if (arg.empty())
        return {.success = false, .error = "missing number"};

    int num = -1;
    try {
        num = std::stoi(arg);
    } catch (...) {
        return {.success = false, .error = "invalid number"};
    }

    g_pOverview->onKbSelectNumber(num);
    return {};
}

static std::optional<int> tokenToIndex(const std::string& s) {
    if (s.size() != 1)
        return std::nullopt;
    const char c = s[0];
    if (c >= '1' && c <= '9')
        return (c - '1');
    if (c == '0')
        return 9;
    if (c >= 'a' && c <= 'z')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'Z')
        return 10 + (c - 'A');
    return std::nullopt;
}

static SDispatchResult onKbSelectTokenDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};
    while (!arg.empty() && std::isspace(arg.front())) arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back())) arg.pop_back();
    const auto idx = tokenToIndex(arg);
    if (!idx)
        return {.success = false, .error = "invalid token (expected 1..9, 0, a..z)"};
    g_pOverview->onKbSelectToken(*idx);
    return {};
}

static SDispatchResult onKbSelectIndexDispatcher(std::string arg) {
    if (!g_pOverview)
        return {};
    // trim
    while (!arg.empty() && std::isspace(arg.front())) arg.erase(arg.begin());
    while (!arg.empty() && std::isspace(arg.back())) arg.pop_back();
    int idx = -1;
    try { idx = std::stoi(arg); } catch (...) { idx = -1; }
    if (idx <= 0)
        return {.success = false, .error = "invalid index (expected >= 1)"};
    // convert to 0-based visible index
    g_pOverview->onKbSelectToken(idx - 1);
    return {};
}
