# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
    include/pyxel/dialogs.hpp

    src/dialogs.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-dialogs ${SRCS})

target_compile_options(pyxel-dialogs PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-dialogs PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-dialogs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pyxel-dialogs PUBLIC mbits::args)
set_target_properties(pyxel-dialogs PROPERTIES FOLDER libs/backends)
