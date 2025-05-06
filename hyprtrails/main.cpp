#define WLR_USE_UNSTABLE

#include <unistd.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>

#include "globals.hpp"
#include "shaders.hpp"
#include "trail.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

void onNewWindow(void* self, std::any data) {
    // data is guaranteed
    const auto PWINDOW = std::any_cast<PHLWINDOW>(data);

    HyprlandAPI::addWindowDecoration(PHANDLE, PWINDOW, makeUnique<CTrail>(PWINDOW));
}

GLuint CompileShader(const GLuint& type, std::string src) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (ok == GL_FALSE)
        throw std::runtime_error("compileShader() failed!");

    return shader;
}

GLuint CreateProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = CompileShader(GL_VERTEX_SHADER, vert);

    if (!vertCompiled)
        throw std::runtime_error("Compiling vshader failed.");

    auto fragCompiled = CompileShader(GL_FRAGMENT_SHADER, frag);

    if (!fragCompiled)
        throw std::runtime_error("Compiling fshader failed.");

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (ok == GL_FALSE)
        throw std::runtime_error("createProgram() failed! GL_LINK_STATUS not OK!");

    return prog;
}

int onTick(void* data) {
    EMIT_HOOK_EVENT("trailTick", nullptr);

    const int TIMEOUT = g_pHyprRenderer->m_mostHzMonitor ? 1000.0 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
    wl_event_source_timer_update(g_pGlobalState->tick, TIMEOUT);

    return 0;
}

void initGlobal() {
    g_pHyprRenderer->makeEGLCurrent();

    GLuint prog                           = CreateProgram(QUADTRAIL, FRAGTRAIL);
    g_pGlobalState->trailShader.program   = prog;
    g_pGlobalState->trailShader.proj      = glGetUniformLocation(prog, "proj");
    g_pGlobalState->trailShader.tex       = glGetUniformLocation(prog, "tex");
    g_pGlobalState->trailShader.color     = glGetUniformLocation(prog, "color");
    g_pGlobalState->trailShader.texAttrib = glGetAttribLocation(prog, "colors");
    g_pGlobalState->trailShader.posAttrib = glGetAttribLocation(prog, "pos");
    g_pGlobalState->trailShader.gradient  = glGetUniformLocation(prog, "snapshots");

    g_pGlobalState->tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_pGlobalState->tick, 1);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[ht] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)", CHyprColor{1.0, 0.2, 0.2, 1.0},
                                     5000);
        throw std::runtime_error("[ht] Version mismatch");
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:bezier_step", Hyprlang::FLOAT{0.025});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:points_per_step", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:history_points", Hyprlang::INT{20});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:history_step", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:hyprtrails:color", Hyprlang::INT{*configStringToInt("rgba(ffaa00ff)")});

    static auto P = HyprlandAPI::registerCallbackDynamic(PHANDLE, "openWindow", [&](void* self, SCallbackInfo& info, std::any data) { onNewWindow(self, data); });

    g_pGlobalState = makeUnique<SGlobalState>();
    initGlobal();

    // add deco to existing windows
    for (auto& w : g_pCompositor->m_windows) {
        if (w->isHidden() || !w->m_isMapped)
            continue;

        HyprlandAPI::addWindowDecoration(PHANDLE, w, makeUnique<CTrail>(w));
    }

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[hyprtrails] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"hyprtrails", "A plugin to add trails behind moving windows", "Vaxry", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    wl_event_source_remove(g_pGlobalState->tick);
    g_pHyprRenderer->m_renderPass.removeAllOfType("CTrailPassElement");
}
