// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <format>
#include <pyxel/gfx/object.hpp>
#include <pyxel/gfx/sized.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include "sink_lease.hpp"

namespace shapes::glue {
	using pyxel::gfx::length;
	using pyxel::gfx::object;
	using pyxel::gfx::sink;
	using pyxel::gfx::size;
	using pyxel::gfx::sized;

	namespace {
		// lets Python subclasses implement output(); the self-life support
		// keeps the Python part of the object alive after a group takes
		// the ownership over the C++ part
		template <typename Base>
		struct py_output : Base, py::trampoline_self_life_support {
			using Base::Base;

			void output(sink& out, glm::dmat4 const& parent) const override {
				py::gil_scoped_acquire gil{};
				auto const fn = py::get_override(static_cast<Base const*>(this), "output");
				if (!fn) {
					py::object const self = py::cast(static_cast<Base const*>(this));
					auto const type_name = py::type::of(self).attr("__name__").cast<std::string>();
					throw py::type_error(std::format("{}.output: not implemented", type_name));
				}
				// PYBIND11_OVERRIDE would pass a copy of the sink, losing the
				// solids added to it; the lease passes a reference, valid
				// until the call returns
				sink_lease const lease{out};
				fn(lease.py_sink(), parent);
			}
		};

		constexpr auto output_doc =
		    "Adds the geometry to `sink`, transformed by the matrix `m`; must be implemented by the subclasses.";
	}  // namespace

	void bind_object(py::module_& m) {
		py::class_<object, py_output<object>, py::smart_holder>(
		    m, "object",
		    "Base of everything a group can hold; subclass it and implement `output(sink, m)`, then pass "
		    "an instance to `group.add`.")
		    .def(py::init<>())
		    .def("output", &object::output, "sink"_a, "m"_a, output_doc);

		// the modifiers return the same Python object, so the calls can be
		// chained also on the subclasses
		constexpr auto self_ref = py::return_value_policy::reference;

		py::class_<sized, object, py_output<sized>, py::smart_holder>(
		    m, "sized",
		    "Base of the objects defined by their bounding box; subclass it and implement `output(sink, m)`. "
		    "All the modifiers return the object itself, so the calls can be chained.")
		    .def(py::init<size const&>(), "size"_a)
		    .def(py::init<length, length, length>(), "width"_a, "height"_a, "depth"_a)

		    .def_property(
		        "size", [](sized const& self) { return self.size(); },
		        [](sized& self, size const& value) { self.set_size(value); },
		        "Bounding box of the object; a copy, so assign a whole size to change it.")

		    .def(
		        "set_size", [](sized& self, size const& value) -> sized& { return self.set_size(value); }, "size"_a,
		        self_ref, "Sets the `size`; returns the object.")
		    .def(
		        "set_size",
		        [](sized& self, length width, length height, length depth) -> sized& {
			        return self.set_size(width, height, depth);
		        },
		        "width"_a, "height"_a, "depth"_a, self_ref, "Sets the `size` from its edges; returns the object.")
		    .def(
		        "resize",
		        [](sized& self, std::optional<length> width, std::optional<length> height, std::optional<length> depth)
		            -> sized& { return self.resize({.width = width, .height = height, .depth = depth}); },
		        py::kw_only(), "width"_a = py::none(), "height"_a = py::none(), "depth"_a = py::none(), self_ref,
		        "Replaces the given edges, keeping the others; returns the object.")
		    .def(
		        "grow_by", [](sized& self, size const& delta) -> sized& { return self.grow_by(delta); }, "delta"_a,
		        self_ref, "Adds `delta` to the edges; returns the object.")
		    .def(
		        "grow_by",
		        [](sized& self, length dwidth, length dheight, length ddepth) -> sized& {
			        return self.grow_by(dwidth, dheight, ddepth);
		        },
		        "dwidth"_a, "dheight"_a, "ddepth"_a, self_ref,
		        "Adds the given lengths to the edges; returns the object.")
		    .def(
		        "scale",
		        [](sized& self, std::optional<double> x, std::optional<double> y, std::optional<double> z) -> sized& {
			        return self.scale(size::factors{.x = x, .y = y, .z = z});
		        },
		        py::kw_only(), "x"_a = py::none(), "y"_a = py::none(), "z"_a = py::none(), self_ref,
		        "Multiplies each edge by its own factor, keeping the edges without one; returns the object.")
		    .def(
		        "scale", [](sized& self, double factor) -> sized& { return self.scale(factor); }, "factor"_a, self_ref,
		        "Multiplies all the edges by `factor`; returns the object.");
	}
}  // namespace shapes::glue
