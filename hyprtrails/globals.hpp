#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

struct SGlobalState {
    SShader          trailShader;
    wl_event_source* tick = nullptr;
};

inline UP<SGlobalState> g_pGlobalState;
