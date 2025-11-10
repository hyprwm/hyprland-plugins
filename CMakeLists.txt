cmake_minimum_required(VERSION 3.27)
project(hyprland-plugins
    VERSION 0.51.0
    DESCRIPTION "Official plugins for Hyprland"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(borders-plus-plus)
add_subdirectory(csgo-vulkan-fix)
add_subdirectory(hyprbars)
add_subdirectory(hyprexpo)
add_subdirectory(hyprfocus)
add_subdirectory(hyprscrolling)
add_subdirectory(hyprtrails)
add_subdirectory(hyprwinwrap)
add_subdirectory(xtra-dispatchers)
