// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <format>
#include <functional>
#include <memory>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/solid.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <shapes/glue/glue.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "locked.hpp"

namespace shapes::glue {
	using pyxel::gfx::group;
	using pyxel::gfx::light;
	using pyxel::gfx::object;
	using pyxel::gfx::offset;
	using pyxel::gfx::solid;
	using pyxel::gfx::solid_object;
	using matrix = glm::dmat4;
	namespace ia = pyxel::gfx::interactions;

	void bind_group(py::module_& m) {
		// every group is owned by its C++ parent (or by the host, for the
		// top-level one), never by Python: there is no constructor, and all
		// the groups are returned by reference; the smart holder is required
		// by the base class, which Python can subclass
		constexpr auto ref = py::return_value_policy::reference;
		// children are kept by their parents, so a child handed to Python
		// must not outlive the parent it came from
		constexpr auto child_ref = py::return_value_policy::reference_internal;

		py::class_<group, object, py::smart_holder>(
		    m, "group",
		    "Named, painted and positioned container of other objects. Groups are owned by their parents and "
		    "only provided by the host or by `add_group`, and placed with `transform` and the `matrix` operations. "
		    "They are only valid until the model function they were given to returns; a group kept for later "
		    "raises ReferenceError. "
		    "A named group becomes a node of the exported model, keeping its transformation (with the ones of "
		    "the unnamed groups above it); the transformation of an unnamed group is baked into the vertices. "
		    "So the transformation reaching a named group must be made of a translation, a rotation and a scale "
		    "only: no shear, e.g. no non-uniform scale after a rotation. A negative scale (mirroring) is fine. "
		    "All the `set_...` methods and `transform` return the group itself, so the calls can be chained.")

		    // name and material
		    .def_property(
		        "name", [](group const& self) { return self.name(); },
		        [](group& self, std::string name) { self.set_name(std::move(name)); },
		        "Name of the group, nesting its contents in the object path; empty adds no level.")
		    .def("set_name", &group::set_name, "name"_a, ref, "Sets the `name`; returns the group.")

		    .def_property(
		        "material", [](group const& self) { return self.material(); },
		        [](group& self, std::string material) { self.set_material(std::move(material)); },
		        "Material of the group contents; empty keeps the material of the parent.")
		    .def("set_material", &group::set_material, "material"_a, ref, "Sets the `material`; returns the group.")

		    // transformation
		    .def(
		        "transform", [](group& self, matrix const& m) -> group& { return self.transform(m); }, "m"_a, ref,
		        "Applies the matrix `m` after the transformations already in the group, e.g. "
		        "`grp.transform(I.rotate_z(deg(90)).translate(cm(10)))`; returns the group. The group starts with "
		        "the move to its `pos`.")
		    .def(
		        "set_transform", [](group& self, matrix const& m) -> group& { return self.set_transform(m); }, "m"_a,
		        ref, "Replaces the transformation of the group, including the initial move; returns the group.")

		    // result
		    .def("current_matrix", py::overload_cast<>(&group::current_matrix, py::const_),
		         "Local to parent transform: the move to `pos`, followed by everything given to `transform`.")
		    .def("current_matrix", py::overload_cast<glm::dmat4 const&>(&group::current_matrix, py::const_), "parent"_a,
		         "Local to world transform, given the transform of the parent.")

		    // children
		    .def(
		        "add_group",
		        [](group& self, offset const& pos, std::string name, std::string material) -> group& {
			        return self.add_group({.pos = pos, .name = std::move(name), .material = std::move(material)});
		        },
		        py::kw_only(), "pos"_a = offset{}, "name"_a = "", "material"_a = "", child_ref,
		        "Adds an empty child group and returns it; the child is owned by this group.")
		    // the solids and lights go first, the last overload takes any
		    // Python object
		    .def(
		        "add", [](group& self, light const& value) { self.add(value); }, "light"_a,
		        "Adds a copy of the light, placed by the transformation of this group, as for any other child; "
		        "later changes to `light` do not affect the copy, and the same light can be added many times. "
		        "The light shines along the -z of that transformation. Returns None.")
		    .def(
		        "add", [](group& self, solid const& shape) { self.add<solid_object>(shape); }, "shape"_a,
		        "Adds a copy of the solid, e.g. a result of `union` or `solid.cube`; later changes to `shape` do not "
		        "affect the copy, and the same solid can be added many times. Returns None.")
		    .def(
		        "add",
		        [](group& self, py::object const& child) -> object& {
			        if (!py::isinstance<object>(child)) {
				        throw py::type_error(
				            std::format("group.add: expected a `shapes.object` or a `shapes.solid`, not {}",
				                        py::type::of(child).attr("__name__").cast<std::string>()));
			        }
			        // taking the ownership fails for the objects already added
			        // and for the groups, which always belong to their parents
			        auto const already_owned = [&] {
				        return py::value_error(
				            std::format("group.add: `{}` already belongs to a group and cannot be added again",
				                        py::repr(child).cast<std::string>()));
			        };

			        std::unique_ptr<object> owned{};
			        try {
				        owned = child.cast<std::unique_ptr<object>>();
			        } catch (std::invalid_argument const&) {
				        // a group, or another object with a non-owning holder
				        throw already_owned();
			        } catch (std::runtime_error const&) {
				        // an object already added
				        throw already_owned();
			        }
			        return self.add(std::move(owned));
		        },
		        "child"_a, child_ref,
		        "Adds the object, e.g. an instance of a `sized` subclass, and returns it; from now on, the child is "
		        "owned by this group and cannot be added anywhere else.")

		    // active objects; the list of the group is changed and read
		    // under its critical section (see locked.hpp), the arguments
		    // are copied before
		    .def(
		        "add_active_object",
		        [](group& self, std::string name, std::vector<ia::slot> slots,
		           std::vector<std::reference_wrapper<ia::state const>> const& states, std::vector<ia::action> actions,
		           ia::transitions const& transitions) -> group& {
			        // slots and actions are immutable, states and transitions
			        // are copied under their own critical sections
			        ia::geo_active_object declaration{
			            .name = std::move(name),
			            .slots = std::move(slots),
			            .actions = std::move(actions),
			            .transitions = copy_of(transitions),
			        };
			        declaration.states.reserve(states.size());
			        for (auto const& state : states)
				        declaration.states.push_back(copy_of(state.get()));
			        return change(self, [&](group& value) { value.add_active_object(std::move(declaration)); });
		        },
		        py::kw_only(), "name"_a, py::arg_v("slots", std::vector<ia::slot>{}, "[]"),
		        py::arg_v("states", std::vector<std::reference_wrapper<ia::state const>>{}, "[]"),
		        py::arg_v("actions", std::vector<ia::action>{}, "[]"),
		        py::arg_v("transitions", ia::transitions{}, "transitions()"), ref,
		        "Declares a geometry-based active object, keeping a copy of it; returns the group. Nothing is "
		        "checked here: the groups and slots it names are checked when the model is exported, which "
		        "fails, if the declaration breaks the rules of docs/model-interactions.md.")
		    .def(
		        "set_light_transition",
		        [](group& self, std::string name, std::string rule) -> group& {
			        return change(self,
			                      [&](group& value) { value.set_light_transition(std::move(name), std::move(rule)); });
		        },
		        "name"_a, "rule"_a, ref,
		        "Declares the transition of the intensity of the light `name`, e.g. `\"150ms ease-out\"`, "
		        "replacing the one declared on this group before; returns the group.")
		    .def_property_readonly(
		        "active_objects",
		        [](group const& self) { return read(self, [](group const& value) { return value.active_objects(); }); },
		        "Copies of the declarations, `geo_active_object` and `light_active_object`, in the order they "
		        "were added.");
	}
}  // namespace shapes::glue
