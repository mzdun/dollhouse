# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SHAPES_PACKAGE_DIR ${PROJECT_BINARY_DIR}/bin/shapes)

set(SRCS
    include/shapes/__main__/main.hpp
    include/shapes/glue/glue.hpp
    include/shapes/library.hpp
    src/__init__.cpp
    src/__main__/config.cpp
    src/__main__/config.hpp
    src/__main__/main.cpp
    src/export.cpp
    src/export.hpp
    src/glue/bind_angle.cpp
    src/glue/bind_area.cpp
    src/glue/bind_axis.cpp
    src/glue/bind_color.cpp
    src/glue/bind_dmat4.cpp
    src/glue/bind_group.cpp
    src/glue/bind_length.cpp
    src/glue/bind_library.cpp
    src/glue/bind_light.cpp
    src/glue/bind_main.cpp
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

# Environment of the Python interpreter loading the module from this build
# tree (on sanitized builds: the ASan preload, see dollhouse_python_env)
dollhouse_python_env(SHAPES_PYTHON_ENV)

# On sanitized MSVC builds, the module needs the ASan runtime next to it
# before anything (stubs, tests) imports it
dollhouse_copy_asan_runtime(shapes)

# #################################################################
# #  STUBS
# #################################################################
# The .pyi stubs go next to the module, from where `flow dyn-install` copies
# them into site-packages
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pybind11_stubgen"
    RESULT_VARIABLE SHAPES_STUBGEN_MISSING
    OUTPUT_QUIET ERROR_QUIET
)
if(SHAPES_STUBGEN_MISSING)
    message(WARNING "pybind11-stubgen not found for ${Python3_EXECUTABLE}; stubs of `shapes` will not be generated")
else()
    list(JOIN SHAPES_PYTHON_ENV "|" SHAPES_PYTHON_ENV_ARG)
    add_custom_command(TARGET shapes POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DPYTHON=${Python3_EXECUTABLE}
            -DMODULE=shapes
            -DOUTPUT_DIR=${PROJECT_BINARY_DIR}/bin
            "-DPYTHON_ENV=${SHAPES_PYTHON_ENV_ARG}"
            -P ${PROJECT_SOURCE_DIR}/.flow/cmake/genstubs.cmake
        COMMENT "Generating stubs of the shapes module"
        VERBATIM
    )
endif()

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    # pytest suite for the bindings, run against the module in this build
    # tree (not the copy `flow dyn-install` puts into site-packages)
    set(SHAPES_PYTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/python)

    add_test(
        NAME shapes-python
        COMMAND ${Python3_EXECUTABLE} -m pytest
            -c ${SHAPES_PYTEST_DIR}/pytest.ini
            --rootdir ${SHAPES_PYTEST_DIR}
            -o cache_dir=${CMAKE_CURRENT_BINARY_DIR}/.pytest_cache
            "--junitxml=${TEST_REPORT_DIR}/shapes-python/${TEST_REPORT_FILE}"
            ${SHAPES_PYTEST_DIR}
        WORKING_DIRECTORY ${SHAPES_PYTEST_DIR}
    )
    set(SHAPES_PYTEST_ENV
        "SHAPES_MODULE_DIR=${PROJECT_BINARY_DIR}/bin"
        "PYTHONPYCACHEPREFIX=${CMAKE_CURRENT_BINARY_DIR}/pycache"
        ${SHAPES_PYTHON_ENV}
    )
    set_tests_properties(shapes-python PROPERTIES
        LABELS python
        TIMEOUT 300
        ENVIRONMENT "${SHAPES_PYTEST_ENV}"
    )
endif()
