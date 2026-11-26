# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

# the imgui package ships its backends as sources, next to the library
# (res/bindings); the sources are the same in every configuration, so any
# package folder known to CMakeDeps will do
set(IMGUI_BINDINGS_DIR)
foreach(config ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES} Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" config)
  set(dir "${imgui_PACKAGE_FOLDER_${config}}/res/bindings")
  if (imgui_PACKAGE_FOLDER_${config} AND EXISTS "${dir}/imgui_impl_sdlgpu3.cpp")
    set(IMGUI_BINDINGS_DIR "${dir}")
    break()
  endif()
endforeach()
if (NOT IMGUI_BINDINGS_DIR)
  message(FATAL_ERROR "imgui backends not found in the imgui package")
endif()

# add_imgui_binding(<target> <name>... [LINK <libs>...])
#   a static library of the imgui_impl_<name>.cpp files, compiled without
#   the project's warnings
function(add_imgui_binding TARGET)
  cmake_parse_arguments(PARSE_ARGV 1 BND "" "" "LINK")
  set(srcs)
  foreach(name ${BND_UNPARSED_ARGUMENTS})
    file(GLOB files "${IMGUI_BINDINGS_DIR}/imgui_impl_${name}.*" "${IMGUI_BINDINGS_DIR}/imgui_impl_${name}_*.h")
    list(APPEND srcs ${files})
  endforeach()
  add_library(${TARGET} STATIC ${srcs})
  target_include_directories(${TARGET} SYSTEM PUBLIC ${IMGUI_BINDINGS_DIR})
  target_link_libraries(${TARGET} PUBLIC imgui::imgui ${BND_LINK})
  set_target_properties(${TARGET} PROPERTIES FOLDER libs/external)
endfunction()
