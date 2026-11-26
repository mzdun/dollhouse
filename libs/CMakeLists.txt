# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

include(imgui_bindings)

add_subdirectory(pyxel-models)
add_subdirectory(pyxel-io)
add_subdirectory(pyxel-engine)
add_subdirectory(backends)
add_subdirectory(dollhouse)

# only the code with real tests; the backends need a window and a GPU and
# the dollhouse app is a thin entry point
add_to_coverage(
    pyxel-models
    pyxel-io
    pyxel-engine
)

cov_set_parent_scope()
