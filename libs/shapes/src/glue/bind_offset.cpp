// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <format>
#include <optional>
#include <pyxel/gfx.hpp>
#include <shapes/glue/glue.hpp>
#include "length.hpp"

namespace shapes::glue {
	using pyxel::gfx::axis;
	using pyxel::gfx::length;
	using pyxel::gfx::offset;

	void bind_offset(py::module_& m) {
		py::class_<offset>(m, "offset", "Point, or a displacement, in the model space.")
		    .def(py::init([](length x, length y, length z) { return offset{x, y, z}; }), "x"_a = length{},
		         "y"_a = length{}, "z"_a = length{})
		    .def_readwrite("x", &offset::x)
		    .def_readwrite("y", &offset::y)
		    .def_readwrite("z", &offset::z)

		    .def(
		        "replace",
		        [](offset const& self, std::optional<length> x, std::optional<length> y, std::optional<length> z) {
			        return self.replace({.x = x, .y = y, .z = z});
		        },
		        py::kw_only(), "x"_a = py::none(), "y"_a = py::none(), "z"_a = py::none(),
		        "Copy of this offset, with the given coordinates replaced.")
		    .def("apply", &offset::apply, "transform"_a,
		         "Transforms this offset as a point, so it is affected by translation.")
		    .def("apply_direction", &offset::apply_direction, "transform"_a,
		         "Transforms this offset as a direction, so it is not affected by translation.")

		    .def("__getitem__", [](offset const& self, axis a) { return self[a]; })
		    .def("__setitem__", [](offset& self, axis a, length value) { self[a] = value; })

		    .def(py::self == py::self)
		    .def(-py::self)
		    .def(py::self + py::self)
		    .def(py::self - py::self)
		    .def(py::self += py::self)
		    .def(py::self -= py::self)
		    .def(py::self * double())
		    .def(double() * py::self)
		    .def(py::self / double())
		    .def(py::self *= double())
		    .def(py::self /= double())

		    .def("__repr__",
		         [](offset const& self) {
			         return std::format("offset(x={}, y={}, z={})", length_repr(self.x), length_repr(self.y),
			                            length_repr(self.z));
		         })
		    .def("__str__", [](offset const& self) { return std::format("{}", dyn(self)); });

		m.def("at_x", &offset::at_x, "x"_a, "Offset along the x axis only.");
		m.def("at_y", &offset::at_y, "y"_a, "Offset along the y axis only.");
		m.def("at_z", &offset::at_z, "z"_a, "Offset along the z axis only.");
		m.def(
		    "distance", [](offset const& lhs, offset const& rhs) { return distance(lhs, rhs); }, "lhs"_a, "rhs"_a,
		    "Distance between two points.");
		m.def(
		    "distance_squared", [](offset const& lhs, offset const& rhs) { return distance_squared(lhs, rhs); },
		    "lhs"_a, "rhs"_a, "Squared distance between two points.");
		m.def(
		    "lerp", [](offset const& from, offset const& to, double t) { return lerp(from, to, t); }, "start"_a,
		    "end"_a, "t"_a, "Point at `t` of the way from `start` to `end`; `t` outside [0, 1] extrapolates.");
		m.def(
		    "midpoint", [](offset const& lhs, offset const& rhs) { return midpoint(lhs, rhs); }, "lhs"_a, "rhs"_a,
		    "Point halfway between the two points.");
		m.def(
		    "min_pt", [](offset const& lhs, offset const& rhs) { return min_pt(lhs, rhs); }, "lhs"_a, "rhs"_a,
		    "Per-axis minimum of two points.");
		m.def(
		    "max_pt", [](offset const& lhs, offset const& rhs) { return max_pt(lhs, rhs); }, "lhs"_a, "rhs"_a,
		    "Per-axis maximum of two points.");
		m.def(
		    "near", [](offset const& lhs, offset const& rhs, length tolerance) { return near(lhs, rhs, tolerance); },
		    "lhs"_a, "rhs"_a, "tolerance"_a = length{pyxel::math::micrometers{1}},
		    "True, if no coordinate differs by more than `tolerance`.");
	}
}  // namespace shapes::glue
