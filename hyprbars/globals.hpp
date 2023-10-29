#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

struct SHyprButton {
    std::string cmd  = "";
    CColor      col  = CColor(0, 0, 0, 0);
    float       size = 10;
};

struct SGlobalState {
    std::vector<SHyprButton> buttons;
};

inline std::unique_ptr<SGlobalState> g_pGlobalState;