#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Texture.hpp>

inline HANDLE PHANDLE = nullptr;

struct SHyprButton {
    std::string  cmd     = "";
    bool         userfg  = false;
    CHyprColor   fgcol   = CHyprColor(0, 0, 0, 0);
    CHyprColor   bgcol   = CHyprColor(0, 0, 0, 0);
    float        size    = 10;
    std::string  icon    = "";
    SP<CTexture> iconTex = makeShared<CTexture>();
};

class CHyprBar;

struct SGlobalState {
    std::vector<SHyprButton>  buttons;
    std::vector<WP<CHyprBar>> bars;
};

inline UP<SGlobalState> g_pGlobalState;
