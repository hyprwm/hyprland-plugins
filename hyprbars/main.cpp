#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

#include "barDeco.hpp"
#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void onNewWindow(void* self, std::any data) {
    // data is guaranteed
    auto* const PWINDOW = std::any_cast<CWindow*>(data);

    if (!PWINDOW->m_bX11DoesntWantBorders)
        HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, new CHyprBar(PWINDOW));
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, std::any data) { onNewWindow(self, data); });

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_color", SConfigValue{.intValue = configStringToInt("rgba(33333388)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_height", SConfigValue{.intValue = 15});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:col.text", SConfigValue{.intValue = configStringToInt("rgba(ffffffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size", SConfigValue{.intValue = 10});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font", SConfigValue{.strValue = "Sans"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:buttons:col.close", SConfigValue{.intValue = configStringToInt("rgba(cc0000cc)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:buttons:col.maximize", SConfigValue{.intValue = configStringToInt("rgba(ffff33cc)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:buttons:button_size", SConfigValue{.intValue = 10});

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped)
            continue;

        HyprlandAPI::addWindowDecoration(PHANDLE, w.get(), new CHyprBar(w.get()));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprbars", "A plugin to add title bars to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_vMonitors)
        m->scheduledRecalc = true;
}
