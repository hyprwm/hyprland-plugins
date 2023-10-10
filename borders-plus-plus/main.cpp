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

    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, new CBordersPlusPlus(PWINDOW));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:add_borders", SConfigValue{.intValue = 1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_1", SConfigValue{.intValue = configStringToInt("rgba(000000ee)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:borders-plus-plus:col.border_2", SConfigValue{.intValue = configStringToInt("rgba(000000ee)")});

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, std::any data) { onNewWindow(self, data); });

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped)
            continue;

        HyprlandAPI::addNotification(PHANDLE, "b", CColor{0.2, 1.0, 0.2, 1.0}, 2000);

        HyprlandAPI::addWindowDecoration(PHANDLE, w.get(), new CBordersPlusPlus(w.get()));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[borders-plus-plus] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"borders-plus-plus", "A plugin to add more borders to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}