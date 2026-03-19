#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <src/event/EventBus.hpp>
#include <src/render/Shader.hpp>

inline HANDLE PHANDLE = nullptr;

struct SGlobalState {
    CShader          trailShader;

    struct {
        Event::CEventBus::Event<> trailTick;
    } events;

    wl_event_source* tick = nullptr;
};

inline UP<SGlobalState> g_pGlobalState;
