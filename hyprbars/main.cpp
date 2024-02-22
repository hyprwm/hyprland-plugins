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

    if (!PWINDOW->m_bX11DoesntWantBorders) {
        std::unique_ptr<CHyprBar> bar = std::make_unique<CHyprBar>(PWINDOW);
        g_pGlobalState->bars.push_back(bar.get());
        HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, std::move(bar));
    }
}

void onPreConfigReload() {
    g_pGlobalState->buttons.clear();
}

Hyprlang::CParseResult onNewButton(const char* K, const char* V) {
    std::string            v = V;
    CVarList               vars(v);

    Hyprlang::CParseResult result;

    // hyprbars-button = color, size, icon, action

    if (vars[0].empty() || vars[1].empty()) {
        result.setError("var 1 and 2 cannot be empty");
        return result;
    }

    float size = 10;
    try {
        size = std::stof(vars[1]);
    } catch (std::exception& e) {
        result.setError("failed parsing var 2 as int");
        return result;
    }

    g_pGlobalState->buttons.push_back(SHyprButton{vars[3], configStringToInt(vars[0]), size, vars[2]});

    for (auto& b : g_pGlobalState->bars) {
        b->m_bButtonsDirty = true;
    }

    return result;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hb] Version mismatch");
    }

    g_pGlobalState = std::make_unique<SGlobalState>();

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_color", Hyprlang::INT{configStringToInt("rgba(33333388)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_height", Hyprlang::INT{15});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:col.text", Hyprlang::INT{configStringToInt("rgba(ffffffff)")});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_size", Hyprlang::INT{10});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_title_enabled", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_font", Hyprlang::STRING{"Sans"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_text_align", Hyprlang::STRING{"center"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_part_of_window", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_precedence_over_border", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_buttons_alignment", Hyprlang::STRING{"right"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_padding", Hyprlang::INT{7});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprbars:bar_button_padding", Hyprlang::INT{5});

    HyprlandAPI::addConfigKeyword(PHANDLE, "hyprbars-button", onNewButton, Hyprlang::SHandlerOptions{});
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "preConfigReload", [&](void* self, SCallbackInfo& info, std::any data) { onPreConfigReload(); });

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->isHidden() || !w->m_bIsMapped)
            continue;

        onNewWindow(nullptr /* unused */, std::any(w.get()));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprbars", "A plugin to add title bars to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    for (auto& m : g_pCompositor->m_vMonitors)
        m->scheduledRecalc = true;
}
