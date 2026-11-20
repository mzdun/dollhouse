# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

# Generates the stubs of a Python dynamic module with pybind11-stubgen,
# importing the module from OUTPUT_DIR, and writes them next to the binary.
#
#     cmake -DPYTHON=<python> -DMODULE=<name> -DOUTPUT_DIR=<dir>
#           [-DPYTHON_ENV=NAME=value|NAME=value...]
#           [-DLAUNCHER=command|arg...] -P genstubs.cmake
#
# PYTHON_ENV is extra environment for the interpreter (see
# dollhouse_python_env()), with entries separated by `|`. LAUNCHER is the
# command prefix (see DOLLHOUSE_LAUNCHER), with arguments separated by `|`;
# it runs before the environment is set, so it never gets PYTHON_ENV.

foreach(var PYTHON MODULE OUTPUT_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "genstubs: ${var} is not set")
  endif()
endforeach()

string(REPLACE "|" ";" PYTHON_ENV "${PYTHON_ENV}")
string(REPLACE "|" ";" LAUNCHER "${LAUNCHER}")
file(REAL_PATH "${OUTPUT_DIR}" OUTPUT_DIR)

# the module comes from OUTPUT_DIR, never from site-packages
set(run_python
  ${LAUNCHER}
  ${CMAKE_COMMAND} -E env
    --modify "PYTHONPATH=path_list_prepend:${OUTPUT_DIR}"
    ${PYTHON_ENV}
  ${PYTHON}
)

execute_process(
  COMMAND ${run_python} -c "import ${MODULE}; print(${MODULE}.__file__)"
  WORKING_DIRECTORY ${OUTPUT_DIR}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE loaded_from
  ERROR_VARIABLE error
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "${error}\ngenstubs: cannot import `${MODULE}` from ${OUTPUT_DIR}")
endif()
file(REAL_PATH "${loaded_from}" loaded_from)
cmake_path(IS_PREFIX OUTPUT_DIR "${loaded_from}" NORMALIZE from_output_dir)
if(NOT from_output_dir)
  message(FATAL_ERROR "genstubs: expected `${MODULE}` from ${OUTPUT_DIR}, got ${loaded_from}")
endif()

# stubs of removed submodules must not linger
macro(locate_stubs OUT)
  file(GLOB_RECURSE ${OUT} LIST_DIRECTORIES false "${OUTPUT_DIR}/${MODULE}/*.pyi")
  if(EXISTS "${OUTPUT_DIR}/${MODULE}.pyi")
    list(APPEND ${OUT} "${OUTPUT_DIR}/${MODULE}.pyi")
  endif()
endmacro()

locate_stubs(stubs)
if(stubs)
  file(REMOVE ${stubs})
endif()

execute_process(
  COMMAND ${run_python} -m pybind11_stubgen ${MODULE} -o ${OUTPUT_DIR}
  WORKING_DIRECTORY ${OUTPUT_DIR}
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "genstubs: pybind11-stubgen failed for `${MODULE}`")
endif()

locate_stubs(stubs)
if(NOT stubs)
  message(FATAL_ERROR "genstubs: no stubs written for `${MODULE}` in ${OUTPUT_DIR}")
endif()

# pybind11-stubgen types some values (e.g. static properties) after their
# runtime class, which in case of std::filesystem::path is the concrete,
# platform-specific flavor of a path; keep the stubs the same on every
# platform
foreach(stub ${stubs})
  file(READ "${stub}" text)
  string(REGEX REPLACE "pathlib\\.(Windows|Posix)Path" "pathlib.Path" patched "${text}")
  if(NOT patched STREQUAL text)
    file(WRITE "${stub}" "${patched}")
  endif()
endforeach()
