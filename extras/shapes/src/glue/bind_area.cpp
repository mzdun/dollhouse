// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/math/value.hpp>
#include <ratio>
#include "finite.hpp"
#include "measure.hpp"

namespace shapes::glue {
	using pyxel::gfx::length;
	using area = pyxel::math::area<double>;

	void bind_area(py::module_& m) {
		bind_measure<area, pyxel::math::area<double, std::ratio<1, 10'000>>,
		             pyxel::math::area<double, std::ratio<1, 1'000'000>>>(
		    m, {
		           .name = "area",
		           .doc = "Area in the model space; e.g. `area(cm2=1.5)`, or a product of two lengths.",
		           .base_unit = "square meters",
		           .cm_arg = "cm2",
		           .mm_arg = "mm2",
		           .cm_symbol = "cm²",
		       });

		// length * length; `length` is bound before `area`
		py::reinterpret_borrow<py::class_<length>>(m.attr("length"))
		    .def(
		        "__mul__", [](length const& self, length const& rhs) { return checked(self * rhs, "area"); },
		        py::is_operator());
	}
}  // namespace shapes::glue
