# Copyright (c) 2026 Marcin Zdun
# This file is licensed under MIT license (see LICENSE for details)

if (DOLLHOUSE_WITH_SHAPES)
add_subdirectory(shapes)
add_to_coverage(shapes)
endif(DOLLHOUSE_WITH_SHAPES)

if (DOLLHOUSE_WITH_VERIFY)
add_subdirectory(verify)
add_to_coverage(verify)
endif(DOLLHOUSE_WITH_VERIFY)

if (DOLLHOUSE_WITH_INSPECTOR)
add_subdirectory(inspector)
endif(DOLLHOUSE_WITH_INSPECTOR)

cov_set_parent_scope()
