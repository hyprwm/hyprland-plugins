#pragma once

#include <array>

#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

inline HANDLE PHANDLE = nullptr;

struct SVars {
    SP<Config::Values::CIntValue>                  addBorders;
    SP<Config::Values::CBoolValue>                 naturalRounding;
    std::array<SP<Config::Values::CColorValue>, 9> borderColors;
    std::array<SP<Config::Values::CIntValue>, 9>   borderSizes;
};

inline SVars vars = {};
