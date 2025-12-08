#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/includes.hpp>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#undef private

#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::String;

#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

//

static SDispatchResult moveOrExec(std::string in) {
    CVarList vars(in, 0, ',');

    auto focusState = Desktop::focusState();
    auto monitor = focusState->monitor();
    
    if (!monitor || !monitor->m_activeWorkspace)
        return SDispatchResult{.success = false, .error = "No active workspace"};

    const auto PWINDOW = g_pCompositor->getWindowByRegex(vars[0]);

    if (!PWINDOW)
        g_pKeybindManager->spawn(vars[1]);
    else {
        if (monitor->m_activeWorkspace != PWINDOW->m_workspace)
            g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, monitor->m_activeWorkspace);
        else
            g_pCompositor->warpCursorTo(PWINDOW->middle());
        focusState->fullWindowFocus(PWINDOW);
    }

    return SDispatchResult{};
}

static SDispatchResult throwUnfocused(std::string in) {
    const auto [id, name, isAutoID] = getWorkspaceIDNameFromString(in);

    if (id == WORKSPACE_INVALID)
        return SDispatchResult{.success = false, .error = "Failed to find workspace"};

    auto focusState = Desktop::focusState();
    auto window = focusState->window();

    if (!window || !window->m_workspace)
        return SDispatchResult{.success = false, .error = "No valid last window"};

    auto pWorkspace = g_pCompositor->getWorkspaceByID(id);
    if (!pWorkspace)
        pWorkspace = g_pCompositor->createNewWorkspace(id, window->m_workspace->monitorID(), name, false);

    for (const auto& w : g_pCompositor->m_windows) {
        if (w == window || w->m_workspace != window->m_workspace)
            continue;

        g_pCompositor->moveWindowToWorkspaceSafe(w, pWorkspace);
    }

    return SDispatchResult{};
}

static SDispatchResult bringAllFrom(std::string in) {
    const auto [id, name, isAutoID] = getWorkspaceIDNameFromString(in);

    if (id == WORKSPACE_INVALID)
        return SDispatchResult{.success = false, .error = "Failed to find workspace"};

    auto focusState = Desktop::focusState();
    auto monitor = focusState->monitor();
    auto window = focusState->window();
    
    if (!monitor || !monitor->m_activeWorkspace)
        return SDispatchResult{.success = false, .error = "No active monitor"};

    auto pWorkspace = g_pCompositor->getWorkspaceByID(id);
    if (!pWorkspace)
        return SDispatchResult{.success = false, .error = "Workspace isnt open"};

    const auto PLASTWINDOW = window;

    for (const auto& w : g_pCompositor->m_windows) {
        if (w->m_workspace != pWorkspace)
            continue;

        g_pCompositor->moveWindowToWorkspaceSafe(w, monitor->m_activeWorkspace);
    }

    if (PLASTWINDOW) {
        Desktop::focusState()->fullWindowFocus(PLASTWINDOW);
        g_pCompositor->warpCursorTo(PLASTWINDOW->middle());
    }

    return SDispatchResult{};
}

static SDispatchResult closeUnfocused(std::string in) {
	auto focusState = Desktop::focusState();
	auto monitor = focusState->monitor();
	auto window = focusState->window();
	
    if (!window)
        return SDispatchResult{.success = false, .error = "No focused monitor"};

    for (const auto& w : g_pCompositor->m_windows) {
        if (w->m_workspace != monitor->m_activeWorkspace || w->m_monitor != monitor || !w->m_isMapped || w == window)
            continue;

        g_pCompositor->closeWindow(w);
    }

    return SDispatchResult{};
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[xtra-dispatchers] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtd] Version mismatch");
    }

    bool success = true;
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:moveorexec", ::moveOrExec);
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:throwunfocused", ::throwUnfocused);
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:bringallfrom", ::bringAllFrom);
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:closeunfocused", ::closeUnfocused);

    if (success)
        HyprlandAPI::addNotification(PHANDLE, "[xtra-dispatchers] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    else {
        HyprlandAPI::addNotification(PHANDLE, "[xtra-dispatchers] Failure in initialization: failed to register dispatchers", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtd] Dispatchers failed");
    }

    return {"xtra-dispatchers", "A plugin to add some extra dispatchers to hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
