# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

# Describes the build of a Python dynamic module in a JSON file next to it,
# so the copy `flow dyn-install` puts into site-packages, or a package
# installs, can be told apart from the other builds.
#
#     cmake -DOUTPUT=<file> -DBINARY=<name> -DBUILD_TYPE=<config>
#           -DSANITIZER=<ON|OFF> -DFREE_THREADED=<ON|OFF>
#           -DCOMPILER=<id version> -DSYSTEM=<name>
#           -DSOURCE_DIR=<dir> [-DGIT=<git>] -P variant.cmake
#
# FREE_THREADED tells, if the module is declared safe to run without the GIL
# (DOLLHOUSE_FREE_THREADED); without it, a free-threaded interpreter turns the
# GIL on, when importing the module.
#
# The commit is read at build time, not at configure time, so the file
# follows commits made without reconfiguring. The file is written only when
# its content changes.

foreach(var OUTPUT BINARY BUILD_TYPE SANITIZER FREE_THREADED COMPILER SYSTEM SOURCE_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "variant: ${var} is not set")
  endif()
endforeach()

set(commit null)
set(dirty null)
if(GIT)
  execute_process(
    COMMAND ${GIT} rev-parse HEAD
    WORKING_DIRECTORY ${SOURCE_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(result EQUAL 0)
    set(commit "\"${hash}\"")
    execute_process(
      COMMAND ${GIT} status --porcelain
      WORKING_DIRECTORY ${SOURCE_DIR}
      RESULT_VARIABLE result
      OUTPUT_VARIABLE changes
      ERROR_QUIET
    )
    if(NOT result EQUAL 0)
      set(dirty null)
    elseif(changes STREQUAL "")
      set(dirty false)
    else()
      set(dirty true)
    endif()
  endif()
endif()

if(SANITIZER)
  set(sanitizer true)
else()
  set(sanitizer false)
endif()

if(FREE_THREADED)
  set(free_threaded true)
else()
  set(free_threaded false)
endif()

set(content "{
  \"binary\": \"${BINARY}\",
  \"build_type\": \"${BUILD_TYPE}\",
  \"sanitizer\": ${sanitizer},
  \"free_threaded\": ${free_threaded},
  \"compiler\": \"${COMPILER}\",
  \"system\": \"${SYSTEM}\",
  \"commit\": ${commit},
  \"dirty\": ${dirty}
}
")

set(previous)
if(EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" previous)
endif()
if(NOT previous STREQUAL content)
  file(WRITE "${OUTPUT}" "${content}")
endif()
