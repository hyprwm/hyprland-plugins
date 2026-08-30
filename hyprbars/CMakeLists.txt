cmake_minimum_required(VERSION 3.27)

project(hyprbars
    DESCRIPTION "hyprbars plugin for Hyprland"
    VERSION 0.1
)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRC "*.cpp")

add_library(hyprbars SHARED ${SRC})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pixman-1
    wayland-server
    xkbcommon
)

pkg_search_module(LUA REQUIRED IMPORTED_TARGET GLOBAL lua55 lua5.5 lua-55 lua-5.5 lua>=5.5 lua<5.6)
target_include_directories(hyprbars PUBLIC ${LUA_INCLUDE_DIRS})

target_link_libraries(hyprbars PRIVATE rt PkgConfig::deps)

install(TARGETS hyprbars)
