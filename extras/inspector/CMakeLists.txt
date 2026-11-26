# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
  src/main.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_executable(inspector WIN32 ${SRCS})

add_win32_icon(inspector "inspector.ico")
target_compile_options(inspector PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(inspector PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(inspector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(inspector pyxel-sdl3-imgui)
dollhouse_copy_asan_runtime(inspector)
set_target_properties(inspector PROPERTIES
  FOLDER apps
  VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

##################################################################
##  INSTALL
##################################################################

if (DOLLHOUSE_INSTALL)
  install(TARGETS inspector)
endif()
