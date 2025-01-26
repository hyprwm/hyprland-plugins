#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <vector>

#include <hyprland/src/includes.hpp>
#include <any>
#include <sstream>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#undef private

#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// hooks
inline CFunctionHook* subsurfaceHook = nullptr;
inline CFunctionHook* commitHook     = nullptr;
typedef void (*origCommitSubsurface)(CSubsurface* thisptr);
typedef void (*origCommit)(void* owner, void* data);

std::vector<PHLWINDOWREF> bgWindows;

//
void onNewWindow(PHLWINDOW pWindow) {
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:class")->getDataStaticPtr();

    if (pWindow->m_szInitialClass != *PCLASS)
        return;

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    if (!PMONITOR)
        return;

    if (!pWindow->m_bIsFloating)
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(pWindow);

    pWindow->m_vRealSize->setValueAndWarp(PMONITOR->vecSize);
    pWindow->m_vRealPosition->setValueAndWarp(PMONITOR->vecPosition);
    pWindow->m_vSize     = PMONITOR->vecSize;
    pWindow->m_vPosition = PMONITOR->vecPosition;
    pWindow->m_bPinned   = true;
    pWindow->sendWindowSize(pWindow->m_vRealSize->goal(), true);

    bgWindows.push_back(pWindow);

    pWindow->m_bHidden = true; // no renderino hyprland pls

    g_pInputManager->refocus();

    Debug::log(LOG, "[hyprwinwrap] new window moved to bg {}", pWindow);
}

void onCloseWindow(PHLWINDOW pWindow) {
    std::erase_if(bgWindows, [pWindow](const auto& ref) { return ref.expired() || ref.lock() == pWindow; });

    Debug::log(LOG, "[hyprwinwrap] closed window {}", pWindow);
}

void onRenderStage(eRenderStage stage) {
    if (stage != RENDER_PRE_WINDOWS)
        return;

    for (auto& bg : bgWindows) {
        const auto bgw = bg.lock();

        if (bgw->m_pMonitor != g_pHyprOpenGL->m_RenderData.pMonitor)
            continue;

        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // cant use setHidden cuz that sends suspended and shit too that would be laggy
        bgw->m_bHidden = false;

        g_pHyprRenderer->renderWindow(bgw, g_pHyprOpenGL->m_RenderData.pMonitor.lock(), &now, false, RENDER_PASS_ALL, false, true);

        bgw->m_bHidden = true;
    }
}

void onCommitSubsurface(CSubsurface* thisptr) {
    const auto PWINDOW = thisptr->m_pWLSurface->getWindow();

    if (!PWINDOW || std::find_if(bgWindows.begin(), bgWindows.end(), [PWINDOW](const auto& ref) { return ref.lock() == PWINDOW; }) == bgWindows.end()) {
        ((origCommitSubsurface)subsurfaceHook->m_pOriginal)(thisptr);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_bHidden = false;

    ((origCommitSubsurface)subsurfaceHook->m_pOriginal)(thisptr);
    if (const auto MON = PWINDOW->m_pMonitor.lock(); MON)
        g_pHyprOpenGL->markBlurDirtyForMonitor(MON);

    PWINDOW->m_bHidden = true;
}

void onCommit(void* owner, void* data) {
    const auto PWINDOW = ((CWindow*)owner)->m_pSelf.lock();

    if (std::find_if(bgWindows.begin(), bgWindows.end(), [PWINDOW](const auto& ref) { return ref.lock() == PWINDOW; }) == bgWindows.end()) {
        ((origCommit)commitHook->m_pOriginal)(owner, data);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_bHidden = false;

    ((origCommit)commitHook->m_pOriginal)(owner, data);
    if (const auto MON = PWINDOW->m_pMonitor.lock(); MON)
        g_pHyprOpenGL->markBlurDirtyForMonitor(MON);

    PWINDOW->m_bHidden = true;
}

void onConfigReloaded() {
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:class")->getDataStaticPtr();
    g_pConfigManager->parseKeyword("windowrulev2", std::string{"float, class:^("} + *PCLASS + ")$");
    g_pConfigManager->parseKeyword("windowrulev2", std::string{"size 100\% 100\%, class:^("} + *PCLASS + ")$");
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
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

    auto fns = HyprlandAPI::findFunctionsByName(PHANDLE, "onCommit");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: onCommit not found");
    for (auto& fn : fns) {
        if (!fn.demangled.contains("CSubsurface"))
            continue;
        subsurfaceHook = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, (void*)&onCommitSubsurface);
    }

    fns = HyprlandAPI::findFunctionsByName(PHANDLE, "listener_commitWindow");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: listener_commitWindow not found");
    commitHook = HyprlandAPI::createFunctionHook(PHANDLE, fns[0].address, (void*)&onCommit);

    bool hkResult = subsurfaceHook->hook();
    hkResult      = hkResult && commitHook->hook();

    if (!hkResult)
        throw std::runtime_error("hyprwinwrap: hooks failed");

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:class", Hyprlang::STRING{"kitty-bg"});

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinwrap] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprwinwrap", "A clone of xwinwrap for Hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}
