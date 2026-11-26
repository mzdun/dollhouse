# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

set(SRCS
    include/pyxel/io/export.hpp
    include/pyxel/io/import.hpp
    include/pyxel/io/interactions.hpp
    include/pyxel/io/pose.hpp

    src/export.cpp
    src/glb.cpp
    src/glb.hpp
    src/gltf2.cpp
    src/gltf2.hpp
    src/import.cpp
    src/interactions.cpp
    src/interactions_json.hpp
    src/interactions_parse.cpp
    src/interactions_rules.cpp
    src/interactions_rules.hpp
    src/pose.cpp
    src/sources.cpp
    src/sources.hpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCS})

add_library(pyxel-io ${SRCS})

target_compile_options(pyxel-io PRIVATE ${DOLLHOUSE_ADDITIONAL_COMPILE_FLAGS})
target_link_options(pyxel-io PUBLIC ${DOLLHOUSE_ADDITIONAL_LINK_FLAGS})
target_include_directories(pyxel-io
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# the public headers use pyxel-models types; Assimp and RapidJSON stay inside
target_link_libraries(pyxel-io
    PUBLIC pyxel-models
    PRIVATE assimp::assimp rapidjson
)
set_target_properties(pyxel-io PROPERTIES
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}

    # linked into the `shapes` Python module, a shared library
    POSITION_INDEPENDENT_CODE ON
)

# #################################################################
# #  TESTING
# #################################################################
if(DOLLHOUSE_TESTING)
    set(TEST_SRCS
        tests/gltf2.test.cpp
        tests/import.test.cpp
        tests/interactions.test.cpp
        tests/interactions_rules.test.cpp
        tests/pose.test.cpp
        tests/sources.test.cpp
    )
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_SRCS})

    add_project_test(pyxel-io ${TEST_SRCS})
    # the importer is private to the library, as is Assimp
    target_include_directories(pyxel-io-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(pyxel-io-test PUBLIC pyxel-io assimp::assimp rapidjson GTest::gmock_main)

    add_test(NAME pyxel-io COMMAND pyxel-io-test "--gtest_output=xml:${TEST_REPORT_DIR}/pyxel-io/${TEST_REPORT_FILE}")
endif()
