#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/config/shared/parserUtils/ParserUtils.hpp>

#include "borderDeco.hpp"
#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onNewWindow(PHLWINDOW window) {
    HyprlandAPI::addWindowDecoration(PHANDLE, window, makeUnique<CBordersPlusPlus>(window));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[borders-plus-plus] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[bpp] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:natural_rounding", Hyprlang::INT{1});

    for (size_t i = 0; i < 9; ++i) {
        HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_" + std::to_string(i + 1), Hyprlang::INT{*Config::ParserUtils::parseColor("rgba(000000ee)")});
        HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1), Hyprlang::INT{-1});
    }

    HyprlandAPI::reloadConfig();

    static auto P = Event::bus()->m_events.window.open.listen([&](PHLWINDOW w) { onNewWindow(w); });

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        HyprlandAPI::addWindowDecoration(PHANDLE, w, makeUnique<CBordersPlusPlus>(w));
    }

    HyprlandAPI::addNotification(PHANDLE, "[borders-plus-plus] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"borders-plus-plus", "A plugin to add more borders to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_pHyprRenderer->m_renderPass.removeAllOfType("CBorderPPPassElement");
}
