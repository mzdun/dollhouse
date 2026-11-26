# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
    include/pyxel/ui/context.hpp

    src/context.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-ui ${SRCS})

target_compile_options(pyxel-ui PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-ui PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-ui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# panels call ImGui directly, so it is public
target_link_libraries(pyxel-ui PUBLIC pyxel-engine imgui::imgui)

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    set(TEST_SRCS
        tests/context.test.cpp
    )
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

    add_imgui_binding(imgui-null null)

    add_project_test(pyxel-ui ${TEST_SRCS})
    target_link_libraries(pyxel-ui-test PUBLIC pyxel-ui imgui-null GTest::gmock_main)

    add_test(NAME pyxel-ui COMMAND pyxel-ui-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-ui/${TEST_REPORT_FILE}")
endif()
