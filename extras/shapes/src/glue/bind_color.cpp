// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/operators.h>
#include <cstdint>
#include <format>
#include <pyxel/gfx/materials.hpp>
#include <shapes/glue/glue.hpp>
#include "length.hpp"

namespace shapes::glue {
	using pyxel::gfx::color;

	namespace {
		color make_color(uint8_t r, uint8_t g, uint8_t b, float alpha) {
			if (!(alpha >= 0.0f && alpha <= 1.0f)) {
				throw py::value_error(std::format("color: alpha must be within [0, 1], not {}", alpha));
			}
			return pyxel::gfx::rgb(r, g, b, alpha);
		}
	}  // namespace

	void bind_color(py::module_& m) {
		// immutable and hashable, so it can be used as a dict key
		py::class_<color>(m, "color", "sRGB color with linear alpha (0 = transparent, 1 = opaque).")
		    .def(py::init<>(), "Opaque black.")
		    .def(py::init(&make_color), "r"_a, "g"_a, "b"_a, "alpha"_a = 1.0f,
		         "Color from sRGB components in [0, 255] and alpha in [0, 1].")
		    .def_readonly("r", &color::r, "Red component, in [0, 255].")
		    .def_readonly("g", &color::g, "Green component, in [0, 255].")
		    .def_readonly("b", &color::b, "Blue component, in [0, 255].")
		    .def_readonly("alpha", &color::alpha, "Alpha, in [0, 1].")

		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def("__hash__",
		         [](color const& self) { return py::hash(py::make_tuple(self.r, self.g, self.b, self.alpha)); })

		    .def("__repr__", [](color const& self) {
			    // uint8_t would be formatted as a character
			    int const r = self.r, g = self.g, b = self.b;
			    if (self.alpha == 1.0f) return std::format("rgb({}, {}, {})", r, g, b);
			    return std::format("rgb({}, {}, {}, {})", r, g, b, number_repr(self.alpha));
		    });

		m.def("rgb", &make_color, "r"_a, "g"_a, "b"_a, "alpha"_a = 1.0f,
		      "Color from sRGB components in [0, 255] and alpha in [0, 1].");
	}
}  // namespace shapes::glue
