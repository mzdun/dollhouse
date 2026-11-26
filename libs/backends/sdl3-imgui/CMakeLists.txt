# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

# TEMPORARY: feeds ImGui through imgui_impl_sdl3 until pyxel-ui is fed from
# engine events, and puts the SDL3, SDL GPU and ImGui pieces together

add_imgui_binding(imgui-sdl3 sdl3 LINK SDL3::SDL3)

set(SRCS
    include/pyxel/sdl3/imgui_stack.hpp

    src/imgui_stack.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-sdl3-imgui ${SRCS})

target_compile_options(pyxel-sdl3-imgui PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-sdl3-imgui PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-sdl3-imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pyxel-sdl3-imgui PUBLIC pyxel-sdl3-gpu pyxel-ui imgui-sdl3)
set_target_properties(pyxel-sdl3-imgui PROPERTIES FOLDER libs/backends)
