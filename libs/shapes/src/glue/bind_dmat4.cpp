// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <optional>
#include <pyxel/gfx/transform.hpp>
#include <pyxel/math/glm.hpp>
#include <shapes/glue/glue.hpp>

namespace shapes::glue {
	using pyxel::gfx::angle;
	using pyxel::gfx::length;
	using pyxel::gfx::offset;
	using matrix = glm::dmat4;

	// only the class, so `offset` can name it in its signatures
	void bind_dmat4(py::module_& m) {
		py::class_<matrix>(
		    m, "matrix",
		    "4x4 transformation. Start with `I`, the identity, and chain the operations; each returns a new matrix, "
		    "with the new transformation applied after the ones already in it, so "
		    "`I.translate(cm(10)).rotate_z(deg(90))` first moves along x, then rotates onto the y "
		    "axis. Pivots are given in the coordinates after the earlier transformations.");
	}

	// the operations, once `offset` is bound
	void bind_dmat4_ops(py::module_& m) {
		using namespace pyxel::gfx;

		py::reinterpret_borrow<py::class_<matrix>>(m.attr("matrix"))
		    .def(
		        "translate", [](matrix const& self, offset const& delta) { return translate(self, delta); }, "delta"_a,
		        "Adds a move by `delta`.")
		    .def(
		        "translate", [](matrix const& self, length x, length y, length z) { return translate(self, x, y, z); },
		        "x"_a, "y"_a = length{}, "z"_a = length{},
		        "Adds a move by the given distances; `y` and `z` default to 0.")

		    .def(
		        "scale",
		        [](matrix const& self, double x, std::optional<double> y, std::optional<double> z) {
			        return scale(self, x, y, z);
		        },
		        "x"_a, "y"_a = py::none(), "z"_a = py::none(),
		        "Adds scaling by the given factors; `y` and `z` default to `x`, so `scale(2)` is uniform.")

		    .def(
		        "rotate",
		        [](matrix const& self, angle by, double nx, double ny, double nz, offset const& pivot) {
			        return rotate(self, by, {nx, ny, nz}, pivot);
		        },
		        "angle"_a, "nx"_a, "ny"_a, "nz"_a, "pivot"_a,
		        "Adds a rotation by `angle` around the axis (`nx`, `ny`, `nz`) going through `pivot`; "
		        "counter-clockwise, "
		        "when looking from the tip of the axis. The axis needs not be normalized, but must not be zero.")
		    .def(
		        "rotate",
		        [](matrix const& self, angle by, double nx, double ny, double nz, length px, length py, length pz) {
			        return rotate(self, by, {nx, ny, nz}, {px, py, pz});
		        },
		        "angle"_a, "nx"_a, "ny"_a, "nz"_a, "px"_a = length{}, "py"_a = length{}, "pz"_a = length{},
		        "Adds a rotation by `angle` around the axis (`nx`, `ny`, `nz`) going through (`px`, `py`, `pz`), "
		        "which defaults to the origin.")

		    .def(
		        "rotate_x", [](matrix const& self, angle by, offset const& pivot) { return rotate_x(self, by, pivot); },
		        "angle"_a, "pivot"_a, "Adds a rotation by `angle` around the x axis moved to `pivot`.")
		    .def(
		        "rotate_x",
		        [](matrix const& self, angle by, length py, length pz) {
			        return rotate_x(self, by, {length{}, py, pz});
		        },
		        "angle"_a, "py"_a = length{}, "pz"_a = length{},
		        "Adds a rotation by `angle` around the x axis moved to (`py`, `pz`), which defaults to the x axis "
		        "itself.")

		    .def(
		        "rotate_y", [](matrix const& self, angle by, offset const& pivot) { return rotate_y(self, by, pivot); },
		        "angle"_a, "pivot"_a, "Adds a rotation by `angle` around the y axis moved to `pivot`.")
		    .def(
		        "rotate_y",
		        [](matrix const& self, angle by, length px, length pz) {
			        return rotate_y(self, by, {px, length{}, pz});
		        },
		        "angle"_a, "px"_a = length{}, "pz"_a = length{},
		        "Adds a rotation by `angle` around the y axis moved to (`px`, `pz`), which defaults to the y axis "
		        "itself.")

		    .def(
		        "rotate_z", [](matrix const& self, angle by, offset const& pivot) { return rotate_z(self, by, pivot); },
		        "angle"_a, "pivot"_a, "Adds a rotation by `angle` around the z axis moved to `pivot`.")
		    .def(
		        "rotate_z",
		        [](matrix const& self, angle by, length px, length py) {
			        return rotate_z(self, by, {px, py, length{}});
		        },
		        "angle"_a, "px"_a = length{}, "py"_a = length{},
		        "Adds a rotation by `angle` around the z axis moved to (`px`, `py`), which defaults to the z axis "
		        "itself.")

		    .def(
		        "__matmul__", [](matrix const& self, matrix const& other) { return self * other; }, py::is_operator(),
		        "Matrix product `self @ other`: a point is transformed by `other` first, then by `self`, so "
		        "`b @ a` is the same as applying `a`, then `b`. Note the method chains read the other way: "
		        "`m.translate(...)` is `I.translate(...) @ m`.")

		    .def("__eq__", [](matrix const& self, matrix const& other) { return self == other; }, py::is_operator());
		// the identity, as a constant; every operation returns a new matrix,
		// so the shared instance is never changed
		m.attr("I") = identity_matrix;
	}
}  // namespace shapes::glue
