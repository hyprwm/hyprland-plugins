#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/includes.hpp>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
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

    if (!g_pCompositor->m_pLastMonitor || !g_pCompositor->m_pLastMonitor->activeWorkspace)
        return SDispatchResult{.success = false, .error = "No active workspace"};

    const auto PWINDOW = g_pCompositor->getWindowByRegex(vars[0]);

    if (!PWINDOW)
        g_pKeybindManager->spawn(vars[1]);
    else {
        if (g_pCompositor->m_pLastMonitor->activeWorkspace != PWINDOW->m_pWorkspace)
            g_pCompositor->moveWindowToWorkspaceSafe(PWINDOW, g_pCompositor->m_pLastMonitor->activeWorkspace);
        else
            g_pCompositor->warpCursorTo(PWINDOW->middle());
        g_pCompositor->focusWindow(PWINDOW);
    }

    return SDispatchResult{};
}

static SDispatchResult throwUnfocused(std::string in) {
    const auto [id, name] = getWorkspaceIDNameFromString(in);

    if (id == WORKSPACE_INVALID)
        return SDispatchResult{.success = false, .error = "Failed to find workspace"};

    if (!g_pCompositor->m_pLastWindow || !g_pCompositor->m_pLastWindow->m_pWorkspace)
        return SDispatchResult{.success = false, .error = "No valid last window"};

    auto pWorkspace = g_pCompositor->getWorkspaceByID(id);
    if (!pWorkspace)
        pWorkspace = g_pCompositor->createNewWorkspace(id, g_pCompositor->m_pLastWindow->m_pWorkspace->monitorID(), name, false);

    for (const auto& w : g_pCompositor->m_vWindows) {
        if (w == g_pCompositor->m_pLastWindow || w->m_pWorkspace != g_pCompositor->m_pLastWindow->m_pWorkspace)
            continue;

        g_pCompositor->moveWindowToWorkspaceSafe(w, pWorkspace);
    }

    return SDispatchResult{};
}

static SDispatchResult bringAllFrom(std::string in) {
    const auto [id, name] = getWorkspaceIDNameFromString(in);

    if (id == WORKSPACE_INVALID)
        return SDispatchResult{.success = false, .error = "Failed to find workspace"};

    if (!g_pCompositor->m_pLastMonitor || !g_pCompositor->m_pLastMonitor->activeWorkspace)
        return SDispatchResult{.success = false, .error = "No active monitor"};

    auto pWorkspace = g_pCompositor->getWorkspaceByID(id);
    if (!pWorkspace)
        return SDispatchResult{.success = false, .error = "Workspace isnt open"};

    const auto PLASTWINDOW = g_pCompositor->m_pLastWindow.lock();

    for (const auto& w : g_pCompositor->m_vWindows) {
        if (w->m_pWorkspace != pWorkspace)
            continue;

        g_pCompositor->moveWindowToWorkspaceSafe(w, g_pCompositor->m_pLastMonitor->activeWorkspace);
    }

    if (PLASTWINDOW) {
        g_pCompositor->focusWindow(PLASTWINDOW);
        g_pCompositor->warpCursorTo(PLASTWINDOW->middle());
    }

    return SDispatchResult{};
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[xtra-dispatchers] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtd] Version mismatch");
    }

    bool success = true;
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:moveorexec", ::moveOrExec);
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:throwunfocused", ::throwUnfocused);
    success      = success && HyprlandAPI::addDispatcherV2(PHANDLE, "plugin:xtd:bringallfrom", ::bringAllFrom);

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
