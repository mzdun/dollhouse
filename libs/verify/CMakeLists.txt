# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
  src/checks.cpp
  src/checks.hpp
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
