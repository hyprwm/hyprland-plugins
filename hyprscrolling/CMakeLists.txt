cmake_minimum_required(VERSION 3.27)

project(hyprscrolling
    DESCRIPTION "hyprscrolling plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

add_library(hyprscrolling SHARED main.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)
target_link_libraries(hyprscrolling PRIVATE rt PkgConfig::deps)

install(TARGETS hyprscrolling)
