#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <vector>

#include <hyprland/src/includes.hpp>
#include <any>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/event/EventBus.hpp>
#undef private

#include "globals.hpp"

#include <hyprutils/string/VarList.hpp>
#include <hyprutils/animation/BezierCurve.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::Animation;

static struct {
    SP<Config::Values::CBoolValue> onlyOnMonitorChange;
    SP<Config::Values::CFloatValue> fadeOpacity, slideHeight, bounceStrength;
    SP<Config::Values::CStringValue> mode;
} configValues;

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

    if (configValues.onlyOnMonitorChange->value() && lastWindow && lastWindow->m_monitor == window->m_monitor)
        return;

    lastWindow = window;
    const auto        PIN      = Config::animationTree()->getAnimationPropertyConfig("hyprfocusIn");
    const auto        POUT     = Config::animationTree()->getAnimationPropertyConfig("hyprfocusOut");

    if (configValues.mode->value() == "flash") {
        const auto ORIGINAL = window->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->goal();
        window->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setConfig(PIN);
        *window->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE) = configValues.fadeOpacity->value();

        window->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setConfig(POUT);
            *w->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE) = ORIGINAL;

            w->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setCallbackOnEnd(nullptr);
        });
    } else if (configValues.mode->value() == "bounce") {
        const auto ORIGINAL = CBox{window->m_realPosition->goal(), window->m_realSize->goal()};

        window->m_realPosition->setConfig(PIN);
        window->m_realSize->setConfig(PIN);

        auto box = ORIGINAL.copy().scaleFromCenter(configValues.bounceStrength->value());

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
                w->layoutTarget()->recalc();

            w->m_realSize->setCallbackOnEnd(nullptr);
        });
    } else if (configValues.mode->value() == "slide") {
        const auto ORIGINAL = window->m_realPosition->goal();

        window->m_realPosition->setConfig(PIN);

        *window->m_realPosition = ORIGINAL - Vector2D{0.F, configValues.slideHeight->value()};

        window->m_realPosition->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_realPosition->setConfig(POUT);

            if (w->m_isFloating || w->isFullscreen())
                *w->m_realPosition = ORIGINAL;
            else
                w->layoutTarget()->recalc();

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

    static auto P = Event::bus()->m_events.window.active.listen([&](PHLWINDOW w, Desktop::eFocusReason r) { onFocusChange(w); });

    configValues.mode = makeShared<Config::Values::CStringValue>("plugin:hyprfocus:mode", "mode to use", "flash");
    configValues.onlyOnMonitorChange = makeShared<Config::Values::CBoolValue>("plugin:hyprfocus:only_on_monitor_change", "whether to fire the animation only on monitor change", false);
    configValues.fadeOpacity = makeShared<Config::Values::CFloatValue>("plugin:hyprfocus:fade_opacity", "fade opacity", 0.8F, Config::Values::SFloatValueOptions{.min = 0.F, .max = 1.F} );
    configValues.slideHeight = makeShared<Config::Values::CFloatValue>("plugin:hyprfocus:slide_height", "slide height", 20.F, Config::Values::SFloatValueOptions{.min = 0.F, .max = 150.F} );
    configValues.bounceStrength = makeShared<Config::Values::CFloatValue>("plugin:hyprfocus:bounce_strength", "bounce strength", 0.95F, Config::Values::SFloatValueOptions{.min = 0.F, .max = 1.F} );

    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.mode);
    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.onlyOnMonitorChange);
    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.fadeOpacity);
    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.slideHeight);
    HyprlandAPI::addConfigValueV2(PHANDLE, configValues.bounceStrength);

    // this will not be cleaned up after we are unloaded but it doesn't really matter,
    // as if we create this again it will just overwrite the old one.
    Config::animationTree()->m_animationTree.createNode("hyprfocusIn", "windowsIn");
    Config::animationTree()->m_animationTree.createNode("hyprfocusOut", "windowsOut");

    return {"hyprfocus", "Flashfocus for Hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // reset the callbacks to avoid crashes
    for (const auto& w : g_pCompositor->m_windows) {
        if (!validMapped(w))
            continue;

        w->m_realSize->setCallbackOnEnd(nullptr);
        w->m_realPosition->setCallbackOnEnd(nullptr);
        w->alpha(Desktop::View::WINDOW_ALPHA_ACTIVE)->setCallbackOnEnd(nullptr);
    }
}
