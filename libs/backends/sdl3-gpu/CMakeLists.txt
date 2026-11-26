# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

add_imgui_binding(imgui-sdlgpu3 sdlgpu3 LINK SDL3::SDL3)

set(SRCS
    include/pyxel/sdl3/gpu/device.hpp
    include/pyxel/sdl3/gpu/imgui_pass.hpp
    include/pyxel/sdl3/gpu/renderer.hpp

    src/device.cpp
    src/imgui_pass.cpp
    src/renderer.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-sdl3-gpu ${SRCS})

target_compile_options(pyxel-sdl3-gpu PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-sdl3-gpu PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-sdl3-gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pyxel-sdl3-gpu PUBLIC pyxel-sdl3 imgui-sdlgpu3)
set_target_properties(pyxel-sdl3-gpu PROPERTIES FOLDER libs/backends)
