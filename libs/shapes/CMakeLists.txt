# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SHAPES_PACKAGE_DIR ${PROJECT_BINARY_DIR}/bin/shapes)

set(SRCS
    include/shapes/glue/glue.hpp
    include/shapes/library.hpp
    src/__init__.cpp
    src/glue/bind_angle.cpp
    src/glue/bind_area.cpp
    src/glue/bind_axis.cpp
    src/glue/bind_color.cpp
    src/glue/bind_dmat4.cpp
    src/glue/bind_group.cpp
    src/glue/bind_length.cpp
    src/glue/bind_library.cpp
    src/glue/bind_object.cpp
    src/glue/bind_offset.cpp
    src/glue/bind_rng.cpp
    src/glue/bind_sink.cpp
    src/glue/bind_size.cpp
    src/glue/bind_solid.cpp
    src/glue/bind_volume.cpp
    src/glue/length.hpp
    src/glue/lifetime.cpp
    src/glue/lifetime.hpp
    src/glue/measure.hpp
    src/glue/sink_lease.hpp
    src/library.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

pybind11_add_module(shapes SHARED ${SRCS})

set_target_properties(shapes PROPERTIES
    OUTPUT_NAME __init__
    ARCHIVE_OUTPUT_NAME shapes
    PDB_NAME shapes
    RUNTIME_OUTPUT_DIRECTORY ${SHAPES_PACKAGE_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${SHAPES_PACKAGE_DIR}
)

foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(shapes PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${SHAPES_PACKAGE_DIR}
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG} ${SHAPES_PACKAGE_DIR}
    )
endforeach()

target_compile_options(shapes PRIVATE ${PLANNER_ADDITIONAL_COMPILE_FLAGS})
target_link_options(shapes PUBLIC ${PLANNER_ADDITIONAL_LINK_FLAGS})
target_include_directories(shapes
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(shapes PRIVATE pyxel-models mbits::args assimp::assimp)

if(SHAPES_NOTIFIER_SRC)
    target_compile_definitions(shapes PRIVATE SHAPES_HAS_NOTIFIER)
endif()

set_target_properties(shapes PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
set_target_properties(shapes PROPERTIES FOLDER apps)

if(PLANNER_INSTALL)
    install(TARGETS shapes DESTINATION shapes)
endif()
