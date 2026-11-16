# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

cmake_minimum_required (VERSION 4.2)
project (dollhouse
  VERSION 0.1.0
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
set(DOLLHOUSE_W_ERROR OFF CACHE BOOL "Compile with warnings turned to errors")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_BINARY_DIR}/conan" "${PROJECT_SOURCE_DIR}/.flow/cmake")
list(APPEND CMAKE_PREFIX_PATH "${PROJECT_BINARY_DIR}/conan")

include(common)

set(CONAN_CMAKE_SILENT_OUTPUT ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(fmt REQUIRED)
find_package(mbits-args REQUIRED)
find_package(glm REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(assimp REQUIRED)

if (DOLLHOUSE_TESTING)
  enable_testing()

  find_package(GTest REQUIRED)

  set(TEST_REPORT_DIR "${PROJECT_BINARY_DIR}/test-results")
  set(TEST_SANITIZER_TAG "")
  if (DOLLHOUSE_SANITIZE)
    set(TEST_SANITIZER_TAG "-sanitizer")
  endif()
  set(TEST_REPORT_FILE "${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_BUILD_TYPE}${TEST_SANITIZER_TAG}.xml")
endif()

include(output_dirs_setup)
old_conan_output_dirs_setup()

include(${PROJECT_SOURCE_DIR}/.flow/packages/config.cmake)
include(CPack)

add_subdirectory(libs)
