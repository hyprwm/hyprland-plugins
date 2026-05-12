#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <array>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>

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

    vars.addBorders =
        makeShared<Config::Values::CIntValue>("plugin:borders-plus-plus:add_borders", "How many extra borders to draw", 1, Config::Values::SIntValueOptions{.min = 0, .max = 9});
    vars.naturalRounding = makeShared<Config::Values::CBoolValue>("plugin:borders-plus-plus:natural_rounding", "Use the window's original rounding", true);

    HyprlandAPI::addConfigValueV2(PHANDLE, vars.addBorders);
    HyprlandAPI::addConfigValueV2(PHANDLE, vars.naturalRounding);

    static std::array<std::string, 9> borderColorNames;
    static std::array<std::string, 9> borderSizeNames;

    for (size_t i = 0; i < 9; ++i) {
        borderColorNames[i] = "plugin:borders-plus-plus:col.border_" + std::to_string(i + 1);
        borderSizeNames[i]  = "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1);

        vars.borderColors[i] = makeShared<Config::Values::CColorValue>(borderColorNames[i].c_str(), "Color of the extra border", 0xee000000);
        vars.borderSizes[i]  = makeShared<Config::Values::CIntValue>(borderSizeNames[i].c_str(), "Size of the extra border", -1);

        HyprlandAPI::addConfigValueV2(PHANDLE, vars.borderColors[i]);
        HyprlandAPI::addConfigValueV2(PHANDLE, vars.borderSizes[i]);
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
