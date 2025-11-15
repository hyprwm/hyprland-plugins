#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <vector>

#include <hyprland/src/includes.hpp>
#include <any>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#undef private

#include "globals.hpp"

#include <hyprutils/string/VarList.hpp>
#include <hyprutils/animation/BezierCurve.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Animation;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onFocusChange(PHLWINDOW window) {
    if (!window)
        return;

    static PHLWINDOWREF lastWindow;

    if (lastWindow == window)
        return;

    static const auto PONLY_ON_MONITOR_CHANGE = CConfigValue<Hyprlang::INT>("plugin:hyprfocus:only_on_monitor_change");
    if (*PONLY_ON_MONITOR_CHANGE && lastWindow && lastWindow->m_monitor == window->m_monitor)
        return;

    lastWindow = window;

    static const auto POPACITY = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:fade_opacity");
    static const auto PBOUNCE  = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:bounce_strength");
    static const auto PSLIDE   = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:slide_height");
    static const auto PMODE    = CConfigValue<std::string>("plugin:hyprfocus:mode");
    const auto        PIN      = g_pConfigManager->getAnimationPropertyConfig("hyprfocusIn");
    const auto        POUT     = g_pConfigManager->getAnimationPropertyConfig("hyprfocusOut");

    if (*PMODE == "flash") {
        window->m_activeInactiveAlpha->setConfig(PIN);
        *window->m_activeInactiveAlpha = std::clamp(*POPACITY, 0.F, 1.F);

        window->m_activeInactiveAlpha->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_activeInactiveAlpha->setConfig(POUT);
            g_pCompositor->updateWindowAnimatedDecorationValues(w.lock());
            w->updateDynamicRules();

            w->m_activeInactiveAlpha->setCallbackOnEnd(nullptr);
        });
    } else if (*PMODE == "bounce") {
        const auto ORIGINAL = CBox{window->m_realPosition->goal(), window->m_realSize->goal()};

        window->m_realPosition->setConfig(PIN);
        window->m_realSize->setConfig(PIN);

        auto box = ORIGINAL.copy().scaleFromCenter(std::clamp(*PBOUNCE, 0.1F, 1.F));

        *window->m_realPosition = box.pos();
        *window->m_realSize     = box.size();

        window->m_realSize->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_realSize->setConfig(POUT);
            w->m_realPosition->setConfig(POUT);

            if (w->m_isFloating || w->isFullscreen()) {
                *w->m_realPosition = ORIGINAL.pos();
                *w->m_realSize     = ORIGINAL.size();
            } else
                g_pLayoutManager->getCurrentLayout()->recalculateWindow(w.lock());

            w->m_realSize->setCallbackOnEnd(nullptr);
        });
    } else if (*PMODE == "slide") {
        const auto ORIGINAL = window->m_realPosition->goal();

        window->m_realPosition->setConfig(PIN);

        *window->m_realPosition = ORIGINAL - Vector2D{0.F, std::clamp(*PSLIDE, 0.F, 150.F)};

        window->m_realPosition->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_realPosition->setConfig(POUT);

            if (w->m_isFloating || w->isFullscreen())
                *w->m_realPosition = ORIGINAL;
            else
                g_pLayoutManager->getCurrentLayout()->recalculateWindow(w.lock());

            w->m_realPosition->setCallbackOnEnd(nullptr);
        });
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprwinwrap] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hww] Version mismatch");
    }

    // clang-format off
    static auto P  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onFocusChange(std::any_cast<PHLWINDOW>(data)); });
    // clang-format on

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:mode", Hyprlang::STRING{"flash"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:only_on_monitor_change", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:fade_opacity", Hyprlang::FLOAT{0.8F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:slide_height", Hyprlang::FLOAT{20.F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:bounce_strength", Hyprlang::FLOAT{0.95F});

    // this will not be cleaned up after we are unloaded but it doesn't really matter,
    // as if we create this again it will just overwrite the old one.
    g_pConfigManager->m_animationTree.createNode("hyprfocusIn", "windowsIn");
    g_pConfigManager->m_animationTree.createNode("hyprfocusOut", "windowsOut");

    return {"hyprfocus", "Flashfocus for Hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // reset the callbacks to avoid crashes
    for (const auto& w : g_pCompositor->m_windows) {
        if (!validMapped(w))
            continue;

        w->m_realSize->setCallbackOnEnd(nullptr);
        w->m_realPosition->setCallbackOnEnd(nullptr);
        w->m_activeInactiveAlpha->setCallbackOnEnd(nullptr);
    }
}
