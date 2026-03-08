#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Shader.hpp>

inline HANDLE PHANDLE = nullptr;

struct SGlobalState {
    CShader trailShader;
};

inline UP<SGlobalState> g_pGlobalState;
