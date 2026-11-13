# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

configure_file(src/version.hpp.in include/pyxel/version.hpp @ONLY)

set(SRCS
    include/pyxel/math.hpp
    include/pyxel/math/aabb.hpp
    include/pyxel/math/format.hpp
    include/pyxel/math/glm.hpp
    include/pyxel/math/newell_normal.hpp
    include/pyxel/math/rotation.hpp
    include/pyxel/math/value.hpp
    include/pyxel/gfx.hpp
    include/pyxel/gfx/csg.hpp
    include/pyxel/gfx/format.hpp
    include/pyxel/gfx/group.hpp
    include/pyxel/gfx/materials.hpp
    include/pyxel/gfx/object.hpp
    include/pyxel/gfx/sink.hpp
    include/pyxel/gfx/sized.hpp
    include/pyxel/gfx/solid.hpp
    include/pyxel/gfx/solid_object.hpp
    include/pyxel/gfx/space.hpp
    include/pyxel/gfx/transform.hpp
    include/pyxel/rng/dice.hpp
    include/pyxel/rng/engine.hpp
    include/pyxel/rng/noise.hpp
    include/pyxel/rng/spinner.hpp
    include/pyxel/rng/xoshiro.hpp

    src/math/newell_normal.cpp
    src/math/value.cpp
    src/gfx/basic_solids.cpp
    src/gfx/cleanup.cpp
    src/gfx/csg.cpp
    src/gfx/group.cpp
    src/gfx/materials.cpp
    src/gfx/object.cpp
    src/gfx/revolve.cpp
    src/gfx/sink.cpp
    src/gfx/solid.cpp
    src/gfx/space.cpp
    src/gfx/transform.cpp
    src/rng/engine.cpp
    src/rng/noise.cpp

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

target_link_libraries(pyxel-models glm::glm)
set_target_properties(pyxel-models PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}

    # linked into the `shapes` Python module, a shared library
    POSITION_INDEPENDENT_CODE ON
)

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    set(TEST_SRCS
        tests/gfx/basic_solids.test.cpp
        tests/gfx/csg.test.cpp
        tests/gfx/format.test.cpp
        tests/gfx/group.test.cpp
        tests/gfx/revolve.test.cpp
        tests/gfx/sink.test.cpp
        tests/gfx/sized.test.cpp
        tests/gfx/solid.test.cpp
        tests/gfx/solid_object.test.cpp
        tests/gfx/space.test.cpp
        tests/gfx/transform.test.cpp
        tests/math/format.test.cpp
        tests/math/geometry.test.cpp
        tests/math/value.test.cpp
        tests/rng/dice.test.cpp
        tests/rng/engine.test.cpp
        tests/rng/noise.test.cpp
        tests/rng/spinner.test.cpp
        tests/rng/xoshiro.test.cpp
    )
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

    add_project_test(pyxel-models ${TEST_SRCS})
    target_link_libraries(pyxel-models-test PUBLIC pyxel-models GTest::gmock_main fmt::fmt)

    add_test(NAME pyxel-models COMMAND pyxel-models-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-models/${TEST_REPORT_FILE}")
endif()
