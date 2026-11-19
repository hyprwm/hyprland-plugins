#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>
#include <hyprland/src/config/lua/types/LuaConfigColor.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/devices/IKeyboard.hpp>

#include <algorithm>
#include <linux/input-event-codes.h>

#include "barDeco.hpp"
#include "globals.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void onNewWindow(PHLWINDOW window) {
    if (true) {
        if (std::ranges::any_of(window->presentation().decorations(), [](const auto& d) { return d->getDisplayName() == "Hyprbar"; }))
            return;

        auto bar = Hyprutils::Memory::makeShared<CHyprBar>(window);
        g_pGlobalState->bars.emplace_back(bar);
        bar->m_self = bar;
        HyprlandAPI::addWindowDecoration(PHANDLE, window, std::move(bar));
    }
}

static void onPreConfigReload() {
    g_pGlobalState->buttons.clear();
}

static void onConfigReloaded() {
    for (auto& b : g_pGlobalState->bars) {
        if (!b)
            continue;

        b->onConfigReloaded();
    }
}

static void damageButtonsOnSuper() {
    for (auto& b : g_pGlobalState->bars) {
        if (b && b->buttonsOnSuper())
            b->damageEntire();
    }
}

// buttons_on_super: reveal the buttons once Super has been held for buttons_on_super_delay,
// so a quick Super shortcut doesn't flash them on every window
static void onKey(const IKeyboard::SKeyEvent& e) {
    if (e.keycode != KEY_LEFTMETA && e.keycode != KEY_RIGHTMETA)
        return;

    const bool HELD = e.state == WL_KEYBOARD_KEY_STATE_PRESSED;
    if (HELD == g_pGlobalState->superHeld)
        return;

    g_pGlobalState->superHeld = HELD;

    if (!HELD) {
        if (g_pGlobalState->superTimer)
            g_pGlobalState->superTimer->updateTimeout(std::nullopt);

        if (g_pGlobalState->superRevealed) {
            g_pGlobalState->superRevealed = false;
            damageButtonsOnSuper();
        }
        return;
    }

    if (std::ranges::none_of(g_pGlobalState->bars, [](const auto& b) { return b && b->buttonsOnSuper(); }))
        return;

    const auto DELAY = g_pGlobalState->config.buttonsOnSuperDelay->value();
    if (DELAY <= 0) {
        g_pGlobalState->superRevealed = true;
        damageButtonsOnSuper();
        return;
    }

    if (!g_pGlobalState->superTimer) {
        g_pGlobalState->superTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [](SP<CEventLoopTimer>, void*) {
                if (!g_pGlobalState->superHeld)
                    return;

                g_pGlobalState->superRevealed = true;
                damageButtonsOnSuper();
            },
            nullptr);
        g_pEventLoopManager->addTimer(g_pGlobalState->superTimer);
    }

    g_pGlobalState->superTimer->updateTimeout(std::chrono::milliseconds(DELAY));
}

static void onUpdateWindowRules(PHLWINDOW window) {
    const auto BARIT = std::find_if(g_pGlobalState->bars.begin(), g_pGlobalState->bars.end(), [window](const auto& bar) { return bar->getOwner() == window; });

    if (BARIT == g_pGlobalState->bars.end())
        return;

    (*BARIT)->updateRules();
    window->updateWindowData();
}

int newLuaButton(lua_State* L) {
    if (!lua_istable(L, 1))
        return Config::Lua::Bindings::Internal::configError(L, "add_button: expected a table { bg_color, fg_color, size, icon, action }");

    SHyprButton button;

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "bg_color");

        Config::Lua::CLuaConfigColor parser(0);
        auto                         err = parser.parse(L);
        if (err.errorCode != Config::Lua::PARSE_ERROR_OK)
            return Config::Lua::Bindings::Internal::configError(L, "add_button: failed to parse bg_color");

        button.bgcol = parser.parsed();
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "fg_color");

        Config::Lua::CLuaConfigColor parser(0);
        auto                         err = parser.parse(L);
        if (err.errorCode != Config::Lua::PARSE_ERROR_OK)
            return Config::Lua::Bindings::Internal::configError(L, "add_button: failed to parse fg_color");

        button.userfg = true;
        button.fgcol = parser.parsed();
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "size");

        if (!lua_isnumber(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "add_button: size must be an integer");

        button.size = lua_tointeger(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "icon");

        if (!lua_isstring(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "add_button: icon must be a string");

        button.icon = lua_tostring(L, -1);
    }

    {
        Hyprutils::Utils::CScopeGuard x([L] { lua_pop(L, 1); });

        lua_getfield(L, 1, "action");

        if (!lua_isstring(L, -1))
            return Config::Lua::Bindings::Internal::configError(L, "add_button: action must be a string");

        button.cmd = lua_tostring(L, -1);
    }

    g_pGlobalState->buttons.push_back(std::move(button));

    for (auto& b : g_pGlobalState->bars) {
        b->m_bButtonsDirty = true;
    }

    return 0;
}

int setLuaTitle(lua_State* L) {
    const auto WINDOW = Config::Lua::Bindings::Internal::windowFromLuaSelectorOrObject(L, 1, "hl.plugin.hyprbars.set_title");
    if (!WINDOW)
        return Config::Lua::Bindings::Internal::configError(L, "hl.plugin.hyprbars.set_title: expected a window");

    const auto BARIT = std::ranges::find_if(g_pGlobalState->bars, [&WINDOW](const auto& bar) { return bar && bar->getOwner() == WINDOW; });
    if (BARIT == g_pGlobalState->bars.end())
        return Config::Lua::Bindings::Internal::configError(L, "hl.plugin.hyprbars.set_title: the window has no bar");

    (*BARIT)->setTitle(lua_isnoneornil(L, 2) ? std::nullopt : std::optional<std::string>{luaL_checkstring(L, 2)});
    return 0;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprbars] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hb] Version mismatch");
    }

    g_pGlobalState                        = makeUnique<SGlobalState>();
    g_pGlobalState->nobarRuleIdx          = Desktop::Rule::windowEffects()->registerEffect("hyprbars:no_bar");
    g_pGlobalState->barColorRuleIdx       = Desktop::Rule::windowEffects()->registerEffect("hyprbars:bar_color");
    g_pGlobalState->titleColorRuleIdx     = Desktop::Rule::windowEffects()->registerEffect("hyprbars:title_color");
    g_pGlobalState->buttonsOnSuperRuleIdx = Desktop::Rule::windowEffects()->registerEffect("hyprbars:buttons_on_super");

    static auto P  = Event::bus()->m_events.window.open.listen([&](PHLWINDOW w) { onNewWindow(w); });
    static auto P3 = Event::bus()->m_events.window.updateRules.listen([&](PHLWINDOW w) { onUpdateWindowRules(w); });
    static auto P6 = Event::bus()->m_events.input.keyboard.key.listen([&](IKeyboard::SKeyEvent e, Event::SCallbackInfo& info) { onKey(e); });

    g_pGlobalState->config.barColor            = makeShared<Config::Values::CColorValue>("plugin:hyprbars:bar_color", "Change the bar color", 0x88333333);
    g_pGlobalState->config.textColor           = makeShared<Config::Values::CColorValue>("plugin:hyprbars:col.text", "Change the text color", 0xffffffff);
    g_pGlobalState->config.inactiveButtonColor = makeShared<Config::Values::CColorValue>(
        "plugin:hyprbars:inactive_button_color", "Change the inactive button's color. 0x00000000 means unset", 0x00000000);
    g_pGlobalState->config.barHeight       = makeShared<Config::Values::CIntValue>("plugin:hyprbars:bar_height", "Change the bar's height", 15);
    g_pGlobalState->config.barTextSize     = makeShared<Config::Values::CIntValue>("plugin:hyprbars:bar_text_size", "Change the bar's text size", 10);
    g_pGlobalState->config.barTextWeight   = makeShared<Config::Values::CFontWeightValue>("plugin:hyprbars:bar_text_weight", "Bar's title text weight (e.g. \"bold\" or an integer 100-1000)", 400);
    g_pGlobalState->config.barTitleEnabled = makeShared<Config::Values::CBoolValue>("plugin:hyprbars:bar_title_enabled", "Whether to enable titles in the bar", true);
    g_pGlobalState->config.barBlur         = makeShared<Config::Values::CBoolValue>("plugin:hyprbars:bar_blur", "Whether to enable blur of the bar", false);
    g_pGlobalState->config.barTextFont     = makeShared<Config::Values::CStringValue>("plugin:hyprbars:bar_text_font", "Bar's text font", "Sans");
    g_pGlobalState->config.barTextAlign    = makeShared<Config::Values::CStringValue>("plugin:hyprbars:bar_text_align", "Bar's text alignment", "center");
    g_pGlobalState->config.barPartOfWindow =
        makeShared<Config::Values::CBoolValue>("plugin:hyprbars:bar_part_of_window", "Whether the bar is a part of the window (reserves space)", true);
    g_pGlobalState->config.barPrecedenceOverBorder =
        makeShared<Config::Values::CBoolValue>("plugin:hyprbars:bar_precedence_over_border", "Whether the bar is before, or after the border", false);
    g_pGlobalState->config.barButtonsAlignment = makeShared<Config::Values::CStringValue>("plugin:hyprbars:bar_buttons_alignment", "Alignment of the bar buttons", "right");
    g_pGlobalState->config.barPadding          = makeShared<Config::Values::CIntValue>("plugin:hyprbars:bar_padding", "Padding of the bar", 7);
    g_pGlobalState->config.barButtonPadding    = makeShared<Config::Values::CIntValue>("plugin:hyprbars:bar_button_padding", "Padding of the bar buttons", 5);
    g_pGlobalState->config.enabled             = makeShared<Config::Values::CBoolValue>("plugin:hyprbars:enabled", "Whether bars are enabled", true);
    g_pGlobalState->config.iconOnHover         = makeShared<Config::Values::CBoolValue>("plugin:hyprbars:icon_on_hover", "Whether to use an icon on hover of the buttons", false);
    g_pGlobalState->config.buttonsOnHover      = makeShared<Config::Values::CBoolValue>(
        "plugin:hyprbars:buttons_on_hover", "Whether the buttons are only drawn while the cursor is over the bar", false);
    g_pGlobalState->config.onDoubleClick       = makeShared<Config::Values::CStringValue>("plugin:hyprbars:on_double_click", "Action to execute on double click of the bar", "");
    g_pGlobalState->config.barOverlay =
        makeShared<Config::Values::CBoolValue>("plugin:hyprbars:bar_overlay", "Whether the bar is drawn over the top of the window instead of reserving space above it", false);
    g_pGlobalState->config.hideOnCsd =
        makeShared<Config::Values::CBoolValue>("plugin:hyprbars:hide_on_csd", "Whether to leave out windows that draw their own decorations (client-side)", false);
    g_pGlobalState->config.buttonsOnSuper =
        makeShared<Config::Values::CBoolValue>("plugin:hyprbars:buttons_on_super", "Whether the buttons are only drawn while Super is held", false);
    g_pGlobalState->config.buttonsOnSuperDelay =
        makeShared<Config::Values::CIntValue>("plugin:hyprbars:buttons_on_super_delay", "How long Super has to be held before the buttons show, in ms", 250);

    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barColor);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.textColor);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.inactiveButtonColor);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barHeight);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barTextSize);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barTextWeight);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barTitleEnabled);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barBlur);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barTextFont);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barTextAlign);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barPartOfWindow);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barPrecedenceOverBorder);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barButtonsAlignment);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barPadding);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barButtonPadding);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.enabled);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.iconOnHover);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.buttonsOnHover);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.onDoubleClick);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.barOverlay);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.hideOnCsd);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.buttonsOnSuper);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pGlobalState->config.buttonsOnSuperDelay);

    HyprlandAPI::addLuaFunction(PHANDLE, "hyprbars", "add_button", ::newLuaButton);
    HyprlandAPI::addLuaFunction(PHANDLE, "hyprbars", "set_title", ::setLuaTitle);

    static auto P4 = Event::bus()->m_events.config.preReload.listen([&] { onPreConfigReload(); });
    static auto P5 = Event::bus()->m_events.config.reloaded.listen([&] { onConfigReloaded(); });

    // add deco to existing windows
    for (auto& w : Desktop::windowState()->windows()) {
        if (w->isHidden() || !validMapped(w))
            continue;

        onNewWindow(w);
    }

    HyprlandAPI::reloadConfig();

    return {"hyprbars", "A plugin to add title bars to windows.", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // the timer's callback lives in this library
    if (g_pGlobalState->superTimer) {
        g_pEventLoopManager->removeTimer(g_pGlobalState->superTimer);
        g_pGlobalState->superTimer.reset();
    }

    for (auto& m : State::monitorState()->monitors())
        m->m_scheduledRecalc = true;

    g_pHyprRenderer->context().m_pass.removeAllOfType("CBarPassElement");

    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->barColorRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->titleColorRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->nobarRuleIdx);
    Desktop::Rule::windowEffects()->unregisterEffect(g_pGlobalState->buttonsOnSuperRuleIdx);
}
