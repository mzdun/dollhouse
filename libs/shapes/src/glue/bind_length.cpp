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
	using pyxel::gfx::length;

	void bind_length(py::module_& m) {
		// immutable, like Python numbers: no in-place operators, so `a += b`
		// rebinds `a` to a new object instead of changing every alias (e.g.
		// the `x` of the offset it was read from); hence also hashable
		auto cls =
		    py::class_<length>(m, "length", "Distance in the model space; create one with `cm(...)` or `mm(...)`.")
		        .def(py::init<>(), "Zero length.")
		        .def_property_readonly("value", &length::count, "The length, in metres.")
		        .def("to_cm", &to_cm, "The length, in centimetres.")
		        .def("to_mm", &to_mm, "The length, in millimetres.")

		        .def(-py::self)
		        .def("__hash__", [](length const& self) { return py::hash(py::float_(self.count())); })

		        .def(py::self == py::self)
		        .def(py::self != py::self)
		        .def(py::self < py::self)
		        .def(py::self <= py::self)
		        .def(py::self > py::self)
		        .def(py::self >= py::self)

		        .def("__abs__", [](length const& self) { return abs(self); })
		        .def("__str__", [](length const& self) { return std::format("{}", pyxel::math::dyn(self)); })
		        .def("__repr__", [](length const& self) { return length_repr(self); });
		def_checked_sums<length>(cls, "length");
		def_checked_scaling<length>(cls, "length");

		m.def(
		    "cm", [](double value) { return length{pyxel::math::centimeters_f{finite(value, "cm: the value")}}; },
		    "value"_a, "Length of `value` centimetres.");
		m.def(
		    "mm", [](double value) { return length{pyxel::math::millimeters_f{finite(value, "mm: the value")}}; },
		    "value"_a, "Length of `value` millimetres.");
	}
}  // namespace shapes::glue
