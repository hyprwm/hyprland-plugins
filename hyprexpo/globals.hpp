#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <unordered_map>
#include <string>

inline HANDLE PHANDLE = nullptr;

// Track whether we're using scrolling layout
inline bool IS_SCROLLING = false;

// Store monitor-specific workspace method configurations
inline std::unordered_map<std::string, std::string> g_monitorWorkspaceMethods;
