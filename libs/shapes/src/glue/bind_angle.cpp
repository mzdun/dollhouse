// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/operators.h>
#include <format>
#include <pyxel/gfx/space.hpp>
#include <pyxel/math/format.hpp>
#include <shapes/glue/glue.hpp>
#include "length.hpp"

namespace shapes::glue {
	using pyxel::gfx::angle;

	namespace {
		double to_deg(angle value) noexcept { return pyxel::math::degrees{value}.count(); }
	}  // namespace

	void bind_angle(py::module_& m) {
		// immutable and hashable, for the same reasons as `length`
		py::class_<angle>(m, "angle", "Angle in the model space; create one with `deg(...)` or `rad(...)`.")
		    .def(py::init<>(), "Zero angle.")
		    .def_property_readonly("value", &angle::count, "The angle, in radians.")
		    .def("to_rad", &angle::count, "The angle, in radians.")
		    .def("to_deg", &to_deg, "The angle, in degrees.")

		    .def(-py::self)
		    .def(py::self + py::self)
		    .def(py::self - py::self)
		    .def(py::self * double())
		    .def(double() * py::self)
		    .def(py::self / double())
		    .def("__hash__", [](angle const& self) { return py::hash(py::float_(self.count())); })

		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def(py::self < py::self)
		    .def(py::self <= py::self)
		    .def(py::self > py::self)
		    .def(py::self >= py::self)

		    .def("__abs__", [](angle const& self) { return abs(self); })
		    .def("__str__", [](angle const& self) { return std::format("{}", self); })
		    .def("__repr__", [](angle const& self) { return std::format("deg({})", number_repr(to_deg(self))); });

		m.def("rad", &pyxel::gfx::rad, "value"_a, "Angle of `value` radians.");
		m.def("deg", &pyxel::gfx::deg, "value"_a, "Angle of `value` degrees.");
	}
}  // namespace shapes::glue
