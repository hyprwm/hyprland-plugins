#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Shader.hpp>
#include <vector>

class CEventLoopTimer;
class CTrail;

inline HANDLE PHANDLE = nullptr;

struct SGlobalState {
    CShader             trailShader;
    SP<CEventLoopTimer> tick;
    std::vector<CTrail*> trails;
};

inline UP<SGlobalState> g_pGlobalState;
