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
	using pyxel::gfx::size;

	void bind_size(py::module_& m) {
		py::class_<size>(m, "size",
		                 "Dimensions of a box; `width` runs along the x axis, `height` along y and "
		                 "`depth` along z.")
		    .def(py::init([](length width, length height, length depth) { return size{width, height, depth}; }),
		         "width"_a = length{}, "height"_a = length{}, "depth"_a = length{})
		    .def_readwrite("width", &size::width)
		    .def_readwrite("height", &size::height)
		    .def_readwrite("depth", &size::depth)

		    .def_static("cube", &size::cube, "edge"_a, "Box with all three dimensions equal to `edge`.")
		    .def_static("from_corners", &size::from_corners, "min"_a, "max"_a,
		                "Box spanned between two corners; negative, if `max` lies below `min`.")

		    .def(
		        "replace",
		        [](size const& self, std::optional<length> width, std::optional<length> height,
		           std::optional<length> depth) {
			        return self.replace({.width = width, .height = height, .depth = depth});
		        },
		        py::kw_only(), "width"_a = py::none(), "height"_a = py::none(), "depth"_a = py::none(),
		        "Copy of this size, with the given dimensions replaced.")
		    .def(
		        "scaled",
		        [](size const& self, std::optional<double> x, std::optional<double> y, std::optional<double> z) {
			        return self.scaled({.x = x, .y = y, .z = z});
		        },
		        py::kw_only(), "x"_a = py::none(), "y"_a = py::none(), "z"_a = py::none(),
		        "Copy of this size, scaled along the given axes; the missing factors are 1.")
		    .def(
		        "inflate",
		        [](size const& self, std::optional<length> dx, std::optional<length> dy, std::optional<length> dz) {
			        return self.inflate({.x = dx, .y = dy, .z = dz});
		        },
		        py::kw_only(), "dx"_a = py::none(), "dy"_a = py::none(), "dz"_a = py::none(),
		        "Copy of this size, with `dx` added to the width, `dy` to the height and `dz` to the depth; the "
		        "missing ones are zero, the negative ones shrink.")
		    .def("volume", &size::volume, "Volume of the box.")
		    .def("footprint", &size::footprint, "Area covered on the ground (width by depth).")
		    .def("center", &size::center, "Middle of a box with its minimal corner at origin.")
		    .def("to_offset", &size::to_offset,
		         "Offset to the opposite corner of a box with its minimal corner at origin.")

		    .def("__getitem__", [](size const& self, axis a) { return self[a]; })
		    .def("__setitem__", [](size& self, axis a, length value) { self[a] = value; })

		    .def(py::self == py::self)
		    .def(py::self + py::self)
		    .def(py::self - py::self)
		    .def(py::self += py::self)
		    .def(py::self -= py::self)
		    .def(py::self * double())
		    .def(double() * py::self)
		    .def(py::self / double())
		    .def(py::self *= double())
		    .def(py::self /= double())
		    // the opposite corner of a box placed at the offset
		    .def(py::self + offset())
		    .def(offset() + py::self)

		    .def("__repr__",
		         [](size const& self) {
			         return std::format("size(width={}, height={}, depth={})", length_repr(self.width),
			                            length_repr(self.height), length_repr(self.depth));
		         })
		    .def("__str__", [](size const& self) { return std::format("{}", dyn(self)); });
	}
}  // namespace shapes::glue
