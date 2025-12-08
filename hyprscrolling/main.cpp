#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <hyprland/src/includes.hpp>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#undef private

#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::String;

#include "globals.hpp"
#include "Scrolling.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

UP<CScrollingLayout> g_pScrollingLayout;

//

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Version mismatch");
    }

    bool success = true;

    g_pScrollingLayout = makeUnique<CScrollingLayout>();

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:fullscreen_on_one_column", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:column_width", Hyprlang::FLOAT{0.5F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:focus_fit_method", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:follow_focus", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:explicit_column_widths", Hyprlang::STRING{"0.333, 0.5, 0.667, 1.0"});
    HyprlandAPI::addLayout(PHANDLE, "scrolling", g_pScrollingLayout.get());

    if (success)
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    else {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: failed to register dispatchers", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Dispatchers failed");
    }

    return {"hyprscrolling", "A plugin to add a scrolling layout to hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::removeLayout(PHANDLE, g_pScrollingLayout.get());
    g_pScrollingLayout.reset();
}
