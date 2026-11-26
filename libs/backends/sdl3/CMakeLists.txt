# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
    include/pyxel/sdl3/handles.hpp
    include/pyxel/sdl3/platform.hpp
    include/pyxel/sdl3/window.hpp

    src/platform.cpp
    src/window.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-sdl3 ${SRCS})

target_compile_options(pyxel-sdl3 PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-sdl3 PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-sdl3
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(pyxel-sdl3 PUBLIC pyxel-engine SDL3::SDL3)
set_target_properties(pyxel-sdl3 PROPERTIES FOLDER libs/backends)

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    set(TEST_SRCS
        tests/platform.test.cpp
    )
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

    add_project_test(pyxel-sdl3 ${TEST_SRCS})
    target_link_libraries(pyxel-sdl3-test PUBLIC pyxel-sdl3 GTest::gmock_main)

    add_test(NAME pyxel-sdl3 COMMAND pyxel-sdl3-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-sdl3/${TEST_REPORT_FILE}")
endif()
