#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <vector>

#include <hyprland/src/includes.hpp>
#include <any>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private

#include "globals.hpp"

// Do NOT change this function
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// hooks
inline CFunctionHook* subsurfaceHook = nullptr;
inline CFunctionHook* commitHook     = nullptr;
typedef void (*origCommitSubsurface)(Desktop::View::CSubsurface* thisptr);
typedef void (*origCommit)(void* owner, void* data);

std::vector<PHLWINDOWREF> bgWindows;

void                      onNewWindow(PHLWINDOW pWindow) {
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:class")->getDataStaticPtr();
    static auto* const PTITLE = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:title")->getDataStaticPtr();

    static auto* const PSIZEX = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:size_x")->getDataStaticPtr();
    static auto* const PSIZEY = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:size_y")->getDataStaticPtr();
    static auto* const PPOSX  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:pos_x")->getDataStaticPtr();
    static auto* const PPOSY  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:pos_y")->getDataStaticPtr();

    const std::string  classRule(*PCLASS);
    const std::string  titleRule(*PTITLE);

    const bool         classMatches = !classRule.empty() && pWindow->m_initialClass == classRule;
    const bool         titleMatches = !titleRule.empty() && pWindow->m_title == titleRule;

    if (!classMatches && !titleMatches)
        return;

    const auto PMONITOR = pWindow->m_monitor.lock();
    if (!PMONITOR)
        return;

    if (!pWindow->m_isFloating)
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(pWindow);

    float sx = 100.f, sy = 100.f, px = 0.f, py = 0.f;

    try {
        sx = std::stof(*PSIZEX);
    } catch (...) {}
    try {
        sy = std::stof(*PSIZEY);
    } catch (...) {}
    try {
        px = std::stof(*PPOSX);
    } catch (...) {}
    try {
        py = std::stof(*PPOSY);
    } catch (...) {}

    sx = std::clamp(sx, 1.f, 100.f);
    sy = std::clamp(sy, 1.f, 100.f);
    px = std::clamp(px, 0.f, 100.f);
    py = std::clamp(py, 0.f, 100.f);

    if (px + sx > 100.f) {
        Log::logger->log(Log::WARN, "[hyprwinwrap] size_x (%d) + pos_x (%d) > 100, adjusting size_x to %d", sx, px, 100.f - px);
        sx = 100.f - px;
    }
    if (py + sy > 100.f) {
        Log::logger->log(Log::WARN, "[hyprwinwrap] size_y (%d) + pos_y (%d) > 100, adjusting size_y to %d", sy, py, 100.f - py);
        sy = 100.f - py;
    }

    const Vector2D monitorSize = PMONITOR->m_size;
    const Vector2D monitorPos  = PMONITOR->m_position;

    const Vector2D newSize = {static_cast<int>(monitorSize.x * (sx / 100.f)), static_cast<int>(monitorSize.y * (sy / 100.f))};

    const Vector2D newPos = {static_cast<int>(monitorPos.x + (monitorSize.x * (px / 100.f))), static_cast<int>(monitorPos.y + (monitorSize.y * (py / 100.f)))};

    pWindow->m_realSize->setValueAndWarp(newSize);
    pWindow->m_realPosition->setValueAndWarp(newPos);
    pWindow->m_size     = newSize;
    pWindow->m_position = newPos;
    pWindow->m_pinned   = true;
    pWindow->sendWindowSize(true);

    bgWindows.push_back(pWindow);
    pWindow->m_hidden = true;

    g_pInputManager->refocus();
    Log::logger->log(Log::DEBUG, "[hyprwinwrap] new window moved to bg {}", pWindow);
}

void onCloseWindow(PHLWINDOW pWindow) {
    std::erase_if(bgWindows, [pWindow](const auto& ref) { return ref.expired() || ref.lock() == pWindow; });

    Log::logger->log(Log::DEBUG, "[hyprwinwrap] closed window {}", pWindow);
}

void onRenderStage(eRenderStage stage) {
    if (stage != RENDER_PRE_WINDOWS)
        return;

    for (auto& bg : bgWindows) {
        const auto bgw = bg.lock();

        if (bgw->m_monitor != g_pHyprOpenGL->m_renderData.pMonitor)
            continue;

        // cant use setHidden cuz that sends suspended and shit too that would be laggy
        bgw->m_hidden = false;

        g_pHyprRenderer->renderWindow(bgw, g_pHyprOpenGL->m_renderData.pMonitor.lock(), Time::steadyNow(), false, RENDER_PASS_ALL, false, true);

        bgw->m_hidden = true;
    }
}

void onCommitSubsurface(Desktop::View::CSubsurface* thisptr) {
    const auto PWINDOW = Desktop::View::CWindow::fromView(thisptr->wlSurface()->view());

    if (!PWINDOW || std::find_if(bgWindows.begin(), bgWindows.end(), [PWINDOW](const auto& ref) { return ref.lock() == PWINDOW; }) == bgWindows.end()) {
        ((origCommitSubsurface)subsurfaceHook->m_original)(thisptr);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_hidden = false;

    ((origCommitSubsurface)subsurfaceHook->m_original)(thisptr);
    if (const auto MON = PWINDOW->m_monitor.lock(); MON)
        g_pHyprOpenGL->markBlurDirtyForMonitor(MON);

    PWINDOW->m_hidden = true;
}

void onCommit(void* owner, void* data) {
    const auto PWINDOW = ((Desktop::View::CWindow*)owner)->m_self.lock();

    if (std::find_if(bgWindows.begin(), bgWindows.end(), [PWINDOW](const auto& ref) { return ref.lock() == PWINDOW; }) == bgWindows.end()) {
        ((origCommit)commitHook->m_original)(owner, data);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_hidden = false;

    ((origCommit)commitHook->m_original)(owner, data);
    if (const auto MON = PWINDOW->m_monitor.lock(); MON)
        g_pHyprOpenGL->markBlurDirtyForMonitor(MON);

    PWINDOW->m_hidden = true;
}

void onConfigReloaded() {
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:class")->getDataStaticPtr();
    const std::string  classRule(*PCLASS);
    if (!classRule.empty()) {
        g_pConfigManager->parseKeyword("windowrulev2", std::string{"float, class:^("} + classRule + ")$");
        g_pConfigManager->parseKeyword("windowrulev2", std::string{"size 100\% 100\%, class:^("} + classRule + ")$");
    }

    static auto* const PTITLE = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:title")->getDataStaticPtr();
    const std::string  titleRule(*PTITLE);
    if (!titleRule.empty()) {
        g_pConfigManager->parseKeyword("windowrulev2", std::string{"float, title:^("} + titleRule + ")$");
        g_pConfigManager->parseKeyword("windowrulev2", std::string{"size 100\% 100\%, title:^("} + titleRule + ")$");
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
    static auto P  = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(std::any_cast<PHLWINDOW>(data)); });
    static auto P2 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(std::any_cast<PHLWINDOW>(data)); });
    static auto P3 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", [&](void* self, SCallbackInfo& info, std::any data) { onRenderStage(std::any_cast<eRenderStage>(data)); });
    static auto P4 = HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", [&](void* self, SCallbackInfo& info, std::any data) { onConfigReloaded(); });
    // clang-format on

    auto fns = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN7Desktop4View11CSubsurface8onCommitEv");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: onCommit not found");
    subsurfaceHook = HyprlandAPI::createFunctionHook(PHANDLE, fns[0].address, (void*)&onCommitSubsurface);

    fns = HyprlandAPI::findFunctionsByName(PHANDLE, "_ZN7Desktop4View7CWindow12commitWindowEv");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: listener_commitWindow not found");
    commitHook = HyprlandAPI::createFunctionHook(PHANDLE, fns[0].address, (void*)&onCommit);

    bool hkResult = subsurfaceHook->hook();
    hkResult      = hkResult && commitHook->hook();

    if (!hkResult)
        throw std::runtime_error("hyprwinwrap: hooks failed");

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:class", Hyprlang::STRING{"kitty-bg"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:title", Hyprlang::STRING{""});

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:size_x", Hyprlang::STRING{"100"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:size_y", Hyprlang::STRING{"100"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:pos_x", Hyprlang::STRING{"0"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:pos_y", Hyprlang::STRING{"0"});

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinwrap] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprwinwrap", "A clone of xwinwrap for Hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
