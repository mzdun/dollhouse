# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
    include/pyxel/app/application.hpp
    include/pyxel/app/backends.hpp
    include/pyxel/app/driver.hpp
    include/pyxel/app/event.hpp
    include/pyxel/app/main_impl.hpp
    include/pyxel/app/window.hpp
    include/pyxel/camera.hpp
    include/pyxel/handles.hpp

    src/application.cpp
    src/camera.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-engine ${SRCS})

target_compile_options(pyxel-engine PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-engine PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(pyxel-engine PUBLIC pyxel-io mbits::args)

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    set(TEST_SRCS
        tests/camera.test.cpp
        tests/fakes.hpp
        tests/handles.test.cpp
        tests/loop.test.cpp
    )
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

    add_project_test(pyxel-engine ${TEST_SRCS})
    target_include_directories(pyxel-engine-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(pyxel-engine-test PUBLIC pyxel-engine GTest::gmock_main)

    add_test(NAME pyxel-engine COMMAND pyxel-engine-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-engine/${TEST_REPORT_FILE}")
endif()
