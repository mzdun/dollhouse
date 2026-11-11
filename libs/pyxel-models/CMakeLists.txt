# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

configure_file(src/version.hpp.in include/pyxel/version.hpp @ONLY)

set(SRCS
  src/dummy.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/include/pyxel/version.hpp"
)

add_library(pyxel-models ${SRCS})

target_compile_options(pyxel-models PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-models PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-models
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

target_link_libraries(pyxel-models)
set_target_properties(pyxel-models PROPERTIES
  VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

##################################################################
##  TESTING
##################################################################

if (DOLLHOUSE_TESTING)
  set(TEST_SRCS
    tests/dummy.test.cpp
  )
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

  add_project_test(pyxel-models ${TEST_SRCS})
  target_link_libraries(pyxel-models-test PUBLIC pyxel-models GTest::gmock_main fmt::fmt)

  add_test(NAME pyxel-models COMMAND pyxel-models-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-models/${TEST_REPORT_FILE}")
endif()
