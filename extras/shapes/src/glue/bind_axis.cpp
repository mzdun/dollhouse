// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/native_enum.h>
#include <pyxel/gfx/space.hpp>
#include <shapes/glue/glue.hpp>

namespace shapes::glue {
	using pyxel::gfx::axis;

	void bind_axis(py::module_& m) {
		py::native_enum<axis>(m, "axis", "enum.Enum", "One of the three axes of the model space; `y` points up.")
		    .value("x", axis::x)
		    .value("y", axis::y)
		    .value("z", axis::z)
		    .finalize();
	}
}  // namespace shapes::glue
