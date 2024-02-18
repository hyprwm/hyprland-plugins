#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <vector>

#include <hyprland/src/includes.hpp>
#include <any>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#undef private

#include "globals.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// hooks
inline CFunctionHook* damageHook = nullptr;
inline CFunctionHook* commitHook = nullptr;
typedef void (*origDamage)(void* owner, void* data);
typedef void (*origCommit)(void* owner, void* data);

std::vector<CWindow*> bgWindows;

//
void onNewWindow(CWindow* pWindow) {
    static auto* const PCLASS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprwinwrap:class")->getDataStaticPtr();

    if (pWindow->m_szInitialClass != *PCLASS)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR)
        return;

    if (!pWindow->m_bIsFloating)
        g_pLayoutManager->getCurrentLayout()->changeWindowFloatingMode(pWindow);

    pWindow->m_vRealSize.setValueAndWarp(PMONITOR->vecSize);
    pWindow->m_vRealPosition.setValueAndWarp(PMONITOR->vecPosition);
    pWindow->m_vSize     = PMONITOR->vecSize;
    pWindow->m_vPosition = PMONITOR->vecPosition;
    pWindow->m_bPinned   = true;
    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv(), true);

    bgWindows.push_back(pWindow);

    pWindow->m_bHidden = true; // no renderino hyprland pls

    g_pInputManager->refocus();

    Debug::log(LOG, "[hyprwinwrap] new window moved to bg {}", pWindow);
}

void onCloseWindow(CWindow* pWindow) {
    std::erase(bgWindows, pWindow);

    Debug::log(LOG, "[hyprwinwrap] closed window {}", pWindow);
}

void onRenderStage(eRenderStage stage) {
    if (stage != RENDER_PRE_WINDOWS)
        return;

    for (auto& bgw : bgWindows) {
        if (bgw->m_iMonitorID != g_pHyprOpenGL->m_RenderData.pMonitor->ID)
            continue;

        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // cant use setHidden cuz that sends suspended and shit too that would be laggy
        bgw->m_bHidden = false;

        g_pHyprRenderer->renderWindow(bgw, g_pHyprOpenGL->m_RenderData.pMonitor, &now, false, RENDER_PASS_ALL, false, true);

        bgw->m_bHidden = true;
    }
}

void onDamage(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    if (std::find(bgWindows.begin(), bgWindows.end(), PWINDOW) == bgWindows.end()) {
        ((origDamage)damageHook->m_pOriginal)(owner, data);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_bHidden = false;

    ((origDamage)damageHook->m_pOriginal)(owner, data);

    PWINDOW->m_bHidden = true;
}

void onCommit(void* owner, void* data) {
    const auto PWINDOW = (CWindow*)owner;

    if (std::find(bgWindows.begin(), bgWindows.end(), PWINDOW) == bgWindows.end()) {
        ((origCommit)commitHook->m_pOriginal)(owner, data);
        return;
    }

    // cant use setHidden cuz that sends suspended and shit too that would be laggy
    PWINDOW->m_bHidden = false;

    ((origCommit)commitHook->m_pOriginal)(owner, data);

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
                                     CColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hww] Version mismatch");
    }

    HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(std::any_cast<CWindow*>(data)); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "closeWindow", [&](void* self, SCallbackInfo& info, std::any data) { onCloseWindow(std::any_cast<CWindow*>(data)); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "render", [&](void* self, SCallbackInfo& info, std::any data) { onRenderStage(std::any_cast<eRenderStage>(data)); });
    HyprlandAPI::registerCallbackDynamic(PHANDLE, "configReloaded", [&](void* self, SCallbackInfo& info, std::any data) { onConfigReloaded(); });

    auto fns = HyprlandAPI::findFunctionsByName(PHANDLE, "listener_commitSubsurface");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: listener_commitSubsurface not found");
    damageHook = HyprlandAPI::createFunctionHook(PHANDLE, fns[0].address, (void*)&onDamage);

    fns = HyprlandAPI::findFunctionsByName(PHANDLE, "listener_commitWindow");
    if (fns.size() < 1)
        throw std::runtime_error("hyprwinwrap: listener_commitWindow not found");
    commitHook = HyprlandAPI::createFunctionHook(PHANDLE, fns[0].address, (void*)&onCommit);

    bool hkResult = damageHook->hook();
    hkResult      = hkResult && commitHook->hook();

    if (!hkResult)
        throw std::runtime_error("hyprwinwrap: hooks failed");

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprwinwrap:class", Hyprlang::STRING{"kitty-bg"});

    HyprlandAPI::addNotification(PHANDLE, "[hyprwinwrap] Initialized successfully!", CColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprwinwrap", "A clone of xwinwrap for Hyprland", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    ;
}