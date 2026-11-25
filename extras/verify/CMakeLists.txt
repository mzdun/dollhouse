# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
  src/checks.cpp
  src/checks.hpp
  src/emulate.cpp
  src/emulate.hpp
  src/main.cpp
  src/names.cpp
  src/names.hpp
  src/outline.hpp
  src/print.cpp
  src/print.hpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_executable(verify ${SRCS})

target_compile_options(verify PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(verify PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(verify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(verify pyxel-io mbits::args)
dollhouse_copy_asan_runtime(verify)
set_target_properties(verify PROPERTIES
  FOLDER apps
  VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

##################################################################
##  INSTALL
##################################################################

if (DOLLHOUSE_INSTALL)
  install(TARGETS verify)
endif()

##################################################################
##  TESTING
##################################################################

# the presets are named after the build types (see CMakePresets.json),
# which has none for RelWithDebInfo; the cases and their models are in
# tests/, see test-runner in .flow/config.yml
string(TOLOWER "${CMAKE_BUILD_TYPE}" VERIFY_PRESET)
if (DOLLHOUSE_TESTING AND VERIFY_PRESET MATCHES "^(debug|release)$")
  add_test(
    NAME verify-test-runner
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/.flow/flow.py"
      tests runner --preset "${VERIFY_PRESET}"
      --ctrf "${PROJECT_BINARY_DIR}/test-results/ctrf/verify.json"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()
