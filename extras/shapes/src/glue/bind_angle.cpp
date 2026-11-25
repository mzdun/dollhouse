// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/operators.h>
#include <format>
#include <pyxel/gfx/space.hpp>
#include <pyxel/math/format.hpp>
#include <shapes/glue/glue.hpp>
#include "finite.hpp"
#include "length.hpp"

namespace shapes::glue {
	using pyxel::gfx::angle;

	namespace {
		double to_deg(angle value) noexcept { return pyxel::math::degrees{value}.count(); }
	}  // namespace

	void bind_angle(py::module_& m) {
		// immutable and hashable, for the same reasons as `length`
		auto cls =
		    py::class_<angle>(m, "angle", "Angle in the model space; create one with `deg(...)` or `rad(...)`.")
		        .def(py::init<>(), "Zero angle.")
		        .def_property_readonly("value", &angle::count, "The angle, in radians.")
		        .def("to_rad", &angle::count, "The angle, in radians.")
		        .def("to_deg", &to_deg, "The angle, in degrees.")

		        .def(-py::self)
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
		def_checked_sums<angle>(cls, "angle");
		def_checked_scaling<angle>(cls, "angle");

		m.def(
		    "rad", [](double value) { return pyxel::gfx::rad(finite(value, "rad: the value")); }, "value"_a,
		    "Angle of `value` radians.");
		m.def(
		    "deg", [](double value) { return pyxel::gfx::deg(finite(value, "deg: the value")); }, "value"_a,
		    "Angle of `value` degrees.");
	}
}  // namespace shapes::glue
