cmake_minimum_required(VERSION 3.27)

project(csgo-vulkan-fix
    DESCRIPTION "csgo-vulkan-fix plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRC "*.cpp")

add_library(csgo-vulkan-fix SHARED ${SRC})

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
target_link_libraries(csgo-vulkan-fix PRIVATE rt PkgConfig::deps)

install(TARGETS csgo-vulkan-fix)
