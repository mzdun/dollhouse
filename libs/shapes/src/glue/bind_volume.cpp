// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/math/value.hpp>
#include <ratio>
#include "finite.hpp"
#include "measure.hpp"

namespace shapes::glue {
	using pyxel::gfx::length;
	using area = pyxel::math::area<double>;
	using volume = pyxel::math::volume<double>;

	void bind_volume(py::module_& m) {
		bind_measure<volume, pyxel::math::volume<double, std::ratio<1, 1'000'000>>,
		             pyxel::math::volume<double, std::ratio<1, 1'000'000'000>>>(
		    m, {
		           .name = "volume",
		           .doc = "Volume in the model space; e.g. `volume(cm3=1.5)`, or a product of an area and a length.",
		           .base_unit = "cubic metres",
		           .cm_arg = "cm3",
		           .mm_arg = "mm3",
		           .cm_symbol = "cm³",
		       });

		// area * length and length * area; `area` is bound before `volume`
		py::reinterpret_borrow<py::class_<area>>(m.attr("area"))
		    .def(
		        "__mul__", [](area const& self, length const& rhs) { return checked(self * rhs, "volume"); },
		        py::is_operator())
		    .def(
		        "__rmul__", [](area const& self, length const& lhs) { return checked(lhs * self, "volume"); },
		        py::is_operator());
	}
}  // namespace shapes::glue
