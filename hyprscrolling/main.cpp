#define WLR_USE_UNSTABLE

#include <hyprland/src/includes.hpp>
#include <hyprland/src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp>

#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Version mismatch");
    }

    bool success = HyprlandAPI::addTiledAlgo(PHANDLE, "scrolling", &typeid(Layout::Tiled::CScrollingAlgorithm), []() -> UP<Layout::ITiledAlgorithm> {
        return makeUnique<Layout::Tiled::CScrollingAlgorithm>();
    });

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:fullscreen_on_one_column", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:column_width", Hyprlang::FLOAT{0.5F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:focus_fit_method", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:follow_focus", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:follow_debounce_ms", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprscrolling:explicit_column_widths", Hyprlang::STRING{"0.333, 0.5, 0.667, 1.0"});

    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: failed to register dispatchers", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Dispatchers failed");
    }

    return {"hyprscrolling", "A plugin to add a scrolling layout to hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::removeAlgo(PHANDLE, "scrolling");
}
