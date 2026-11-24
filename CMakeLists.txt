# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

cmake_minimum_required (VERSION 4.2)
project (dollhouse
  VERSION 0.3.0
  DESCRIPTION "Your house, your furniture, in one place"
  LANGUAGES CXX)

set(PROJECT_VERSION_SHORT "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(PROJECT_VERSION_STABILITY "" # or "-alpha", or "-beta", or "-rc.1", or "-rc.2"
    CACHE STRING "" FORCE)

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "Building in source directory is not supported")
endif()

set(DOLLHOUSE_TESTING ON CACHE BOOL "Compile and/or run self-tests")
set(DOLLHOUSE_INSTALL ON CACHE BOOL "Install the application")
set(DOLLHOUSE_SANITIZE OFF CACHE BOOL "Compile with sanitizers enabled")
set(DOLLHOUSE_SANITIZE_THREAD OFF CACHE BOOL "Compile with ThreadSanitizer enabled (GCC and Clang)")
set(DOLLHOUSE_W_ERROR OFF CACHE BOOL "Compile with warnings turned to errors")
set(DOLLHOUSE_FREE_THREADED OFF CACHE BOOL "Declare the shapes module safe to run without the GIL (not thread-safe yet)")

# ASan and TSan cannot share a process: each needs the address space laid
# out its own way
if (DOLLHOUSE_SANITIZE AND DOLLHOUSE_SANITIZE_THREAD)
  message(FATAL_ERROR "DOLLHOUSE_SANITIZE and DOLLHOUSE_SANITIZE_THREAD cannot be both ON")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_BINARY_DIR}/conan" "${PROJECT_SOURCE_DIR}/.flow/cmake")
list(APPEND CMAKE_PREFIX_PATH "${PROJECT_BINARY_DIR}/conan")

include(coverage)
include(common)

set(CONAN_CMAKE_SILENT_OUTPUT ON)
# Accept free-threaded (python3.Xt) interpreters as well as regular ones;
# by default FindPython skips them. The fourth ABI element is the
# free-threading flag (CMake 3.30+). Override with -DPython3_FIND_ABI=...
if (NOT DEFINED Python3_FIND_ABI)
  set(Python3_FIND_ABI "ANY" "ANY" "ANY" "ANY")
endif()
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(fmt REQUIRED)
find_package(mbits-args REQUIRED)
find_package(glm REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(assimp REQUIRED)
find_package(RapidJSON REQUIRED)

if (DOLLHOUSE_TESTING)
  enable_testing()

  find_package(GTest REQUIRED)

  set(TEST_REPORT_DIR "${PROJECT_BINARY_DIR}/test-results")
  set(TEST_SANITIZER_TAG "")
  if (DOLLHOUSE_SANITIZE)
    set(TEST_SANITIZER_TAG "-sanitizer")
  elseif (DOLLHOUSE_SANITIZE_THREAD)
    set(TEST_SANITIZER_TAG "-thread-sanitizer")
  endif()
  set(TEST_REPORT_FILE "${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_BUILD_TYPE}${TEST_SANITIZER_TAG}.xml")

  # adds DOLLHOUSE_COVERALLS and, with it ON, the coverage flags of GCC/Clang
  set(COVERALLS_PREFIX DOLLHOUSE_)
  include(${PROJECT_SOURCE_DIR}/tools/coveralls/build_flags.cmake)

  # the gcov counters are not atomic, so every multithreaded test would
  # report races inside the instrumentation
  if (DOLLHOUSE_COVERALLS AND DOLLHOUSE_SANITIZE_THREAD)
    message(FATAL_ERROR "DOLLHOUSE_COVERALLS and DOLLHOUSE_SANITIZE_THREAD cannot be both ON")
  endif()
endif()

include(output_dirs_setup)
old_conan_output_dirs_setup()

include(${PROJECT_SOURCE_DIR}/.flow/packages/config.cmake)
include(CPack)

add_subdirectory(libs)

# #################################################################
# #  COVERAGE
# #################################################################
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/.flow/flow.py"
  system --format props
  OUTPUT_VARIABLE DOLLHOUSE_COV_PROPERTIES
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/.flow/flow.py"
  system --format os
  OUTPUT_VARIABLE DOLLHOUSE_COV_OS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TOLOWER ${CMAKE_CXX_COMPILER_ID} DOLLHOUSE_COV_PROPERTIES_COMPILER)

if(DOLLHOUSE_COV_PROPERTIES_COMPILER STREQUAL "gnu")
  set(DOLLHOUSE_COV_PROPERTIES_COMPILER "gcc")
endif()

string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pbuild_type='${CMAKE_BUILD_TYPE}'")
string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler='${DOLLHOUSE_COV_PROPERTIES_COMPILER}'")

if(MSVC)
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler.version='${MSVC_VERSION}'")
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler.toolset='${CMAKE_VS_PLATFORM_TOOLSET}'")
else()
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler.version='${CMAKE_CXX_COMPILER_VERSION}'")
endif()

if("stdc++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler.libcxx='stdc++'")
elseif("c++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-pcompiler.libcxx='c++'")
endif()

if(DOLLHOUSE_SANITIZE)
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-psanitizer=on")
else()
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-psanitizer=off")
endif()

if(DOLLHOUSE_TESTING)
  set(COVERALLS_PREFIX DOLLHOUSE_)

  set(DOLLHOUSE_COVERALLS_DIRS ${LEGACY_COVERAGE})
  set(DOLLHOUSE_COVERALLS_TARGET dollhouse)

  set(__SANITIZER)

  if(DOLLHOUSE_SANITIZE)
    set(__SANITIZER -sanitizer)
  endif()

  set(DOLLHOUSE_COVERALLS_FILE "${PROJECT_BINARY_DIR}/../artifacts/coverage/coveralls_${DOLLHOUSE_COV_OS}-${CMAKE_BUILD_TYPE}-${DOLLHOUSE_COV_PROPERTIES_COMPILER}${__SANITIZER}.json")
  include(${PROJECT_SOURCE_DIR}/tools/coveralls/Coveralls.cmake)

  file(RELATIVE_PATH COVERALLS_FILE "${PROJECT_SOURCE_DIR}" "${DOLLHOUSE_COVERALLS_FILE}")
  string(APPEND DOLLHOUSE_COV_PROPERTIES "\n-fcoveralls\n${COVERALLS_FILE}")
endif()

# inputs of the `cov` tool (github.com/mzdun/cov), when it is run from the
# build directory
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/report_answers.txt" CONTENT "${DOLLHOUSE_COV_PROPERTIES}\n")
string(REPLACE ";" "\n    include = " SRC_INCLUDE "${COVERED_CODE}")
string(REPLACE ";" "\n    exclude = " SRC_EXCLUDE "${NOT_COVERED_CODE}")
configure_file("${PROJECT_SOURCE_DIR}/.covcollect.in" "${PROJECT_BINARY_DIR}/.covcollect" @ONLY)
