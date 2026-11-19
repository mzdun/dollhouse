// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <format>
#include <optional>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/gfx/solid.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include <vector>

namespace shapes::glue {
	using pyxel::gfx::length;
	using pyxel::gfx::offset;
	using pyxel::gfx::size;
	using pyxel::gfx::solid;

	void bind_solid(py::module_& m) {
		py::class_<solid>(m, "solid",
		                  "Polyhedron made of vertices and faces; each face lists the indices of its vertices.")
		    .def(py::init<>())
		    .def(py::init<std::vector<offset>, std::vector<std::vector<size_t>>>(), "vertices"_a, "faces"_a,
		         "Both vertices and faces are kept as given, duplicates included; de-duplicate them beforehand, if "
		         "needed.")

		    .def_static(
		        "revolve",
		        [](std::vector<solid::profile_point> const& profile, size_t sides) {
			        return solid::revolve(profile, sides);
		        },
		        "profile"_a, "sides"_a,
		        "Solid of revolution: `profile`, a closed polygon of (x, y) pairs of lengths in the XY plane, with x "
		        "being the distance from the Y axis, is copied `sides` times around the Y axis, and the copies are "
		        "joined with faces. The last point is joined back to the first one, segments lying on the axis add no "
		        "faces, and a segment going flat out from the axis becomes a single cap, so "
		        "`[(cm(0), cm(0)), (cm(5), cm(0)), (cm(5), cm(30)), (cm(0), cm(30))]` is a regular prism, "
		        "`[(cm(0), cm(0)), (cm(5), cm(0)), (cm(0), cm(30))]` a regular pyramid, and a profile not touching the "
		        "axis, like `[(cm(3), cm(0)), (cm(5), cm(0)), (cm(5), cm(30)), (cm(3), cm(30))]`, a tube with a "
		        "hole. The coordinates are kept as given, so a profile starting at (0, 0) has the origin in the "
		        "middle of its base. Either direction of the profile gives faces pointing outwards; the profile must "
		        "not cross itself. Raises ValueError, if `sides` is less than 3, if any x is negative, or if the "
		        "profile encloses no area.")

		    // basic solids; the boxes start at the origin, the prism and the
		    // pyramid have it in the middle of the base, the sphere in the center
		    .def_static("cube", &solid::cube, "width"_a, "height"_a, "depth"_a,
		                "Box of the given width (x), height (y) and depth (z), from (0, 0, 0) to (width, height, "
		                "depth). Raises ValueError for sizes not larger than zero.")
		    .def_static(
		        "cube", [](size const& extent) { return solid::cube(extent.width, extent.height, extent.depth); },
		        "extent"_a, "Same as `cube(extent.width, extent.height, extent.depth)`.")
		    .def_static("prism", &solid::prism, "height"_a, "radius"_a, "sides"_a = 20,
		                "Regular prism of `sides` sides around a circle of `radius`, `height` tall, standing on the XZ "
		                "plane, with the origin in the middle of its base. Raises ValueError for sizes not larger than "
		                "zero, or for less than 3 sides.")
		    .def_static(
		        "pyramid", &solid::pyramid, "height"_a, "radius"_a, "sides"_a = 20,
		        "Regular pyramid of `sides` sides around a circle of `radius`, `height` tall, standing on the XZ "
		        "plane, with the origin in the middle of its base. Raises ValueError for sizes not larger than "
		        "zero, or for less than 3 sides.")
		    .def_static("sphere", &solid::sphere, "radius"_a, "longitudes"_a = 32,
		                "UV sphere going from y = -radius to y = radius, centered on the origin, with `longitudes` "
		                "meridians and half as many bands of latitude. Raises ValueError for a radius not larger than "
		                "zero, or for less than 4 longitudes.")
		    .def_static(
		        "oval_sphere", &solid::oval_sphere, "rx"_a, "ry"_a = py::none(), "rz"_a = py::none(),
		        "longitudes"_a = 32,
		        "Sphere stretched along the axes, with the radii `rx`, `ry` and `rz`, centered on the origin; "
		        "the missing radii are equal to `rx`. Same meridians and latitudes as `sphere`. Raises ValueError "
		        "for radii not larger than zero, or for less than 4 longitudes.")
		    .def_static(
		        "rounded_cube", &solid::rounded_cube, "width"_a, "height"_a, "depth"_a, "radius"_a, "segments"_a = 4,
		        "Box like `cube`, with its edges and corners rounded with `radius`, but never more than half of "
		        "the smallest size; `segments` per quarter of a circle. A zero radius gives a plain box. Raises "
		        "ValueError for sizes not larger than zero, a negative radius, or no segments.")
		    .def_static(
		        "rounded_cube",
		        [](size const& extent, length radius, size_t segments) {
			        return solid::rounded_cube(extent.width, extent.height, extent.depth, radius, segments);
		        },
		        "extent"_a, "radius"_a, "segments"_a = 4,
		        "Same as `rounded_cube(extent.width, extent.height, extent.depth, radius, segments)`.")
		    .def_static(
		        "oval_rounded_cube", &solid::oval_rounded_cube, "width"_a, "height"_a, "depth"_a, "rx"_a,
		        "ry"_a = py::none(), "rz"_a = py::none(), "segments"_a = 4,
		        "Box like `cube`, with its edges and corners rounded with separate radii along each axis, each "
		        "never more than half of its own size; the missing radii are equal to `rx`. All radii zero give "
		        "a plain box. `segments` per quarter of an ellipse. Raises ValueError for sizes not larger than "
		        "zero, negative radii, or no segments.")
		    .def_static(
		        "oval_rounded_cube",
		        [](size const& extent, length rx, std::optional<length> ry, std::optional<length> rz, size_t segments) {
			        return solid::oval_rounded_cube(extent.width, extent.height, extent.depth, rx, ry, rz, segments);
		        },
		        "extent"_a, "rx"_a, "ry"_a = py::none(), "rz"_a = py::none(), "segments"_a = 4,
		        "Same as `oval_rounded_cube(extent.width, extent.height, extent.depth, rx, ry, rz, segments)`.")

		    .def_property_readonly("vertices", &solid::vertices, "Copy of the vertices.")
		    .def_property_readonly("faces", &solid::faces, "Copy of the faces, each a list of vertex indices.")

		    .def("reserve", &solid::reserve, "vertex_count"_a, "face_count"_a,
		         "Prepares room for the given number of vertices and faces.")

		    .def("add_vert", py::overload_cast<offset const&>(&solid::add_vert), "v"_a,
		         "Adds the vertex and returns its index; if an equal vertex is already there, returns its index "
		         "instead.")
		    .def("add_vert", py::overload_cast<length, length, length>(&solid::add_vert), "x"_a, "y"_a, "z"_a,
		         "Adds the vertex at the given coordinates and returns its index; if an equal vertex is already "
		         "there, returns its index instead.")

		    // the list overload goes first; a call with separate indices
		    // does not match it and falls through to the variadic one;
		    // overload_cast cannot skip the template, hence static_cast
		    .def("add_face", static_cast<void (solid::*)(std::vector<size_t> const&)>(&solid::add_face), "indexes"_a,
		         "Adds a face from a list of vertex indices. The indices are not checked now, but a face with an "
		         "index past the last vertex will be silently dropped, when the solid is added to a sink.")
		    .def(
		        "add_face",
		        [](solid& self, py::args const& indexes) {
			        std::vector<size_t> face{};
			        face.reserve(indexes.size());
			        for (auto const& index : indexes) {
				        try {
					        face.push_back(index.cast<size_t>());
				        } catch (py::cast_error const&) {
					        throw py::type_error(std::format("solid.add_face: `{}` is not a vertex index",
					                                         py::repr(index).cast<std::string>()));
				        }
			        }
			        self.add_face(face);
		        },
		        // pybind11 cannot name *args, so it is shown as `*args` in the signature
		        "Adds a face from vertex indices given as separate arguments, `add_face(index, ...)`. The indices are "
		        "not checked now, but a face with an index past the last vertex will be silently dropped, when the "
		        "solid is added to a sink.")

		    .def("is_valid", &solid::is_valid,
		         "True, if every face refers only to the vertices added so far. Faces may be added before their "
		         "vertices, so the result can change with later calls to `add_vert`.")

		    .def("transform", &solid::transform, "m"_a,
		         "New solid with every vertex transformed by the matrix `m`; this one is not changed. If `m` "
		         "mirrors, the faces are reversed, so they still face outwards.")

		    // the same operators, as Python's own sets use; no in-place
		    // variants, so `a |= b` rebinds `a` to a new solid
		    .def("__or__", &pyxel::gfx::unite, py::is_operator(), "Same as `union(self, other)`.")
		    .def("__and__", &pyxel::gfx::intersect, py::is_operator(), "Same as `intersection(self, other)`.")
		    .def("__sub__", &pyxel::gfx::subtract, py::is_operator(), "Same as `difference(self, other)`.")
		    .def("__xor__", &pyxel::gfx::symmetric_difference, py::is_operator(),
		         "Same as `symmetric_difference(self, other)`.")

		    .def("__repr__", [](solid const& self) {
			    return std::format("<solid: {} vertices, {} faces>", self.vertices().size(), self.faces().size());
		    });

		// `union` is a C++ keyword, hence the verbs on the C++ side
		static constexpr auto csg_note =
		    " Both solids must be closed, with convex, planar faces, wound counter-clockwise when seen from "
		    "outside; the result is a new solid of the same kind.";
		m.def("union", &pyxel::gfx::unite, "a"_a, "b"_a,
		      std::format("Everything inside `a`, or `b`, or both; same as `a | b`.{}", csg_note).c_str());
		m.def("intersection", &pyxel::gfx::intersect, "a"_a, "b"_a,
		      std::format("Everything inside both `a` and `b`; same as `a & b`.{}", csg_note).c_str());
		m.def("difference", &pyxel::gfx::subtract, "a"_a, "b"_a,
		      std::format("Everything inside `a`, but not inside `b`; same as `a - b`.{}", csg_note).c_str());
		m.def("symmetric_difference", &pyxel::gfx::symmetric_difference, "a"_a, "b"_a,
		      std::format("Everything inside exactly one of `a` and `b`; same as `a ^ b`.{}", csg_note).c_str());

		m.def("cleanup", &pyxel::gfx::cleanup, "obj"_a,
		      "Tidies up the result of one or more of `union`, `intersection`, `difference` and "
		      "`symmetric_difference`, without changing the volume: merges vertices closer than 0.1 um, removes "
		      "T-junctions, merges neighboring faces lying in the same plane (unless the merged face would have a "
		      "hole) and removes vertices in the middle of straight edges. NOTE: after cleanup, no operations are "
		      "allowed; the merged faces may be concave, so the result must not be used with `union`, "
		      "`intersection`, `difference`, `symmetric_difference`, nor with the `|`, `&`, `-`, `^` operators. Do "
		      "all the operations first, then clean up once.");
	}
}  // namespace shapes::glue
