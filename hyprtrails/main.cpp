#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>

#include "globals.hpp"
#include "shaders.hpp"
#include "trail.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void onNewWindow(PHLWINDOW PWINDOW) {
    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, makeUnique<CTrail>(PWINDOW));
}

static void onTick(SP<CEventLoopTimer> self, void* data) {
    tickTrails();

    const int timeoutMs = g_pHyprRenderer->m_mostHzMonitor ? 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
    self->updateTimeout(std::chrono::milliseconds(timeoutMs));
}

void initGlobal() {
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    if (!g_pGlobalState->trailShader.createProgram(QUADTRAIL, FRAGTRAIL, true, false))
        throw std::runtime_error("[ht] Failed to create trail shader");

    g_pGlobalState->tick = makeShared<CEventLoopTimer>(std::chrono::milliseconds(1), onTick, nullptr);
    g_pEventLoopManager->addTimer(g_pGlobalState->tick);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[ht] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)", CHyprColor{1.0, 0.2, 0.2, 1.0},
                                     5000);
        throw std::runtime_error("[ht] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:bezier_step", Hyprlang::FLOAT{0.025});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:points_per_step", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:history_points", Hyprlang::INT{20});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:history_step", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:color", Hyprlang::INT{*configStringToInt("rgba(ffaa00ff)")});

    static auto P = Event::bus()->m_events.window.open.listen([&](PHLWINDOW w) { onNewWindow(w); });

    g_pGlobalState = makeUnique<SGlobalState>();
    initGlobal();

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        HyprlandAPI::addWindowDecoration(PHANDLE, w, makeUnique<CTrail>(w));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprtrails] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprtrails", "A plugin to add trails behind moving windows", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (g_pGlobalState && g_pGlobalState->tick) {
        g_pGlobalState->tick->cancel();
        g_pEventLoopManager->removeTimer(g_pGlobalState->tick);
    }

    g_pHyprRenderer->m_renderPass.removeAllOfType("CTrailPassElement");
}
