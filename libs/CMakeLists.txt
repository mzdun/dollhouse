# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

add_subdirectory(pyxel-models)
add_subdirectory(pyxel-io)
add_subdirectory(pyxel-engine)
add_subdirectory(backends)

if (DOLLHOUSE_WITH_SHAPES)
add_subdirectory(shapes)
endif(DOLLHOUSE_WITH_SHAPES)

add_subdirectory(verify)
add_subdirectory(dollhouse)

# only the code with real tests; pyxel-engine and backends hold
# placeholders and the dollhouse app is a thin entry point
add_to_coverage(
    pyxel-models
    pyxel-io
    verify
)
if (DOLLHOUSE_WITH_SHAPES)
add_to_coverage(shapes)
endif(DOLLHOUSE_WITH_SHAPES)
cov_set_parent_scope()
