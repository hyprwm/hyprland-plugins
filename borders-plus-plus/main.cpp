#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "borderDeco.hpp"
#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void onNewWindow(void* self, std::any data) {
    // data is guaranteed
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::make_unique<CBordersPlusPlus>(PWINDOW));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[borders-plus-plus] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[bpp] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:natural_rounding", Hyprlang::INT{1});

    for (size_t i = 0; i < 9; ++i) {
        HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_" + std::to_string(i + 1), Hyprlang::INT{configStringToInt("rgba(000000ee)")});
        HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:border_size_" + std::to_string(i + 1), Hyprlang::INT{-1});
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped)
            continue;

        HyprlandAPI::addWindowDecoration(PHANDLE, w.get(), std::make_unique<CBordersPlusPlus>(w.get()));
    }

    HyprlandAPI::addNotification(PHANDLE, "[borders-plus-plus] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"borders-plus-plus", "A plugin to add more borders to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}