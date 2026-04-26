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

// Set when a mouse event fires; cleared after each focus change or on keyPress.
static bool g_lastFocusWasMouse = false;

static void onFocusChange(PHLWINDOW window) {
    if (!window)
        return;

    static PHLWINDOWREF lastWindow;

    if (lastWindow == window)
        return;

    lastWindow = window;

    static const auto PENABLE = CConfigValue<Hyprlang::INT>("plugin:hyprfocus:enable");
    if (!*PENABLE)
        return;

    static const auto PANIMATE_FLOATING = CConfigValue<Hyprlang::INT>("plugin:hyprfocus:animate_floating");
    if (!*PANIMATE_FLOATING && window->m_isFloating)
        return;

    static const auto PKEYBOARD_ANIM = CConfigValue<std::string>("plugin:hyprfocus:keyboard_focus_animation");
    static const auto PMOUSE_ANIM    = CConfigValue<std::string>("plugin:hyprfocus:mouse_focus_animation");
    const bool wasMouse = g_lastFocusWasMouse;
    g_lastFocusWasMouse = false;

    const std::string& mode = wasMouse ? *PMOUSE_ANIM : *PKEYBOARD_ANIM;

    static const auto POPACITY = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:fade_opacity");
    static const auto PSHRINK  = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:shrink_percentage");
    static const auto PSLIDE   = CConfigValue<Hyprlang::FLOAT>("plugin:hyprfocus:slide_height");
    const auto        PIN      = g_pConfigManager->getAnimationPropertyConfig("hyprfocusIn");
    const auto        POUT     = g_pConfigManager->getAnimationPropertyConfig("hyprfocusOut");

    if (mode == "flash") {
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
    } else if (mode == "shrink") {
        const auto ORIGINAL = CBox{window->m_realPosition->goal(), window->m_realSize->goal()};

        window->m_realPosition->setConfig(PIN);
        window->m_realSize->setConfig(PIN);

        auto box = ORIGINAL.copy().scaleFromCenter(std::clamp(*PSHRINK, 0.1F, 1.F));

        *window->m_realPosition = box.pos();
        *window->m_realSize     = box.size();

        window->m_realSize->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_realSize->setConfig(POUT);
            w->m_realPosition->setConfig(POUT);

            if (w->m_isFloating) {
                *w->m_realPosition = ORIGINAL.pos();
                *w->m_realSize     = ORIGINAL.size();
            } else
                g_pLayoutManager->getCurrentLayout()->recalculateWindow(w.lock());

            w->m_realSize->setCallbackOnEnd(nullptr);
        });
    } else if (mode == "slide") {
        const auto ORIGINAL = window->m_realPosition->goal();

        window->m_realPosition->setConfig(PIN);

        *window->m_realPosition = ORIGINAL - Vector2D{0.F, std::clamp(*PSLIDE, 0.F, 150.F)};

        window->m_realPosition->setCallbackOnEnd([w = PHLWINDOWREF{window}, POUT, ORIGINAL](WP<CBaseAnimatedVariable> pav) {
            if (!w)
                return;
            w->m_realPosition->setConfig(POUT);

            if (w->m_isFloating)
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
    static auto PM  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseButton", [&](void* self, SCallbackInfo& info, std::any data) { g_lastFocusWasMouse = true; });
    static auto PMM = HyprlandAPI::registerCallbackDynamic(PHANDLE, "mouseMove",   [&](void* self, SCallbackInfo& info, std::any data) { g_lastFocusWasMouse = true; });
    static auto PK  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "keyPress",    [&](void* self, SCallbackInfo& info, std::any data) { g_lastFocusWasMouse = false; });
    static auto P   = HyprlandAPI::registerCallbackDynamic(PHANDLE, "activeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onFocusChange(std::any_cast<PHLWINDOW>(data)); });
    // clang-format on

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:enable", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:animate_floating", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:keyboard_focus_animation", Hyprlang::STRING{"flash"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:mouse_focus_animation", Hyprlang::STRING{"none"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:fade_opacity", Hyprlang::FLOAT{0.8F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:shrink_percentage", Hyprlang::FLOAT{0.95F});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprfocus:slide_height", Hyprlang::FLOAT{20.F});

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
