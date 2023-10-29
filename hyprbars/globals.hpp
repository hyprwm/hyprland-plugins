#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

struct SHyprButton {
    std::string cmd  = "";
    CColor      col  = CColor(0, 0, 0, 0);
    float       size = 10;
    std::string icon = "";
    CTexture    iconTex;
};

class CHyprBar;

struct SGlobalState {
    std::vector<SHyprButton> buttons;
    std::vector<CHyprBar*>   bars;
};

inline std::unique_ptr<SGlobalState> g_pGlobalState;