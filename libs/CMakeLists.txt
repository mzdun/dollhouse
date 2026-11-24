# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

add_subdirectory(pyxel-models)
add_subdirectory(pyxel-io)
add_subdirectory(pyxel-engine)
add_subdirectory(backends)
add_subdirectory(shapes)
add_subdirectory(dollhouse)
add_subdirectory(verify)

# only the code with real tests; pyxel-engine and backends hold
# placeholders and the dollhouse app is a thin entry point
add_to_coverage(
    pyxel-models
    pyxel-io
    shapes
    verify
)
cov_set_parent_scope()
