// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/native_enum.h>
#include <pybind11/stl.h>
#include <algorithm>
#include <format>
#include <optional>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/math/format.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "finite.hpp"
#include "length.hpp"
#include "locked.hpp"

namespace shapes::glue {
	namespace ia = pyxel::gfx::interactions;
	using pyxel::gfx::angle;
	using pyxel::gfx::axis;
	using pyxel::gfx::length;
	using pyxel::gfx::offset;

	namespace {
		ia::slot_value to_slot_value(py::handle value, std::string_view what) {
			// a Python bool is also an int, so it goes first; an int goes to
			// double, as the variant has no integer type (see the `state`
			// helper structure in docs/model-interactions.md)
			if (py::isinstance<py::bool_>(value)) return ia::slot_value{std::in_place_type<bool>, value.cast<bool>()};
			if (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value)) {
				// an int too large for a double raises OverflowError
				auto const number = py::float_{py::reinterpret_borrow<py::object>(value)}.cast<double>();
				return ia::slot_value{std::in_place_type<double>, finite(number, what)};
			}
			if (py::isinstance<angle>(value)) return ia::slot_value{std::in_place_type<angle>, value.cast<angle>()};
			if (py::isinstance<length>(value)) return ia::slot_value{std::in_place_type<length>, value.cast<length>()};
			if (py::isinstance<offset>(value)) return ia::slot_value{std::in_place_type<offset>, value.cast<offset>()};
			throw py::type_error(std::format("{}: expected a bool, a float, an angle, a length or an offset, not `{}`",
			                                 what, py::type::of(value).attr("__name__").cast<std::string>()));
		}

		// a read-only attribute returning a copy: def_readonly would return
		// the units by reference, letting Python change them in place
		template <typename T, typename Value>
		auto copy_member(Value T::* member) {
			return [member](T const& self) -> Value { return self.*member; };
		}

		std::string str_repr(std::string const& value) { return py::repr(py::str(value)).cast<std::string>(); }

		template <typename Value>
		std::string unit_repr(Value const& value) {
			return py::repr(py::cast(value)).cast<std::string>();
		}

		std::string list_repr(std::vector<std::string> const& items) {
			std::string result{"["};
			for (auto const& item : items) {
				if (result.size() > 1) result += ", ";
				result += item;
			}
			return result + "]";
		}

		char axis_name(axis value) {
			switch (value) {
				case axis::y:
					return 'y';
				case axis::z:
					return 'z';
				case axis::x:
					break;
			}
			return 'x';
		}

		// evaluates back to the same change in Python, e.g.
		// "channel.rotate_y(deg(80))"
		std::string change_repr(ia::group_transformation const& change) {
			return std::visit(
			    [](auto const& value) -> std::string {
				    using T = std::remove_cvref_t<decltype(value)>;
				    if constexpr (std::same_as<T, ia::translate_axis>) {
					    return std::format("channel.translate_{}({})", axis_name(value.on), length_repr(value.by));
				    } else if constexpr (std::same_as<T, ia::translate_all>) {
					    return std::format("channel.translate({}, {}, {})", length_repr(value.by.x),
					                       length_repr(value.by.y), length_repr(value.by.z));
				    } else if constexpr (std::same_as<T, ia::rotate_axis>) {
					    return std::format("channel.rotate_{}({})", axis_name(value.around), unit_repr(value.by));
				    } else if constexpr (std::same_as<T, ia::rotate_all>) {
					    return std::format("channel.rotate({}, {}, {})", unit_repr(value.x), unit_repr(value.y),
					                       unit_repr(value.z));
				    } else if constexpr (std::same_as<T, ia::scale_axis>) {
					    return std::format("channel.scale_{}({})", axis_name(value.along), number_repr(value.by));
				    } else {
					    return std::format("channel.scale({}, {}, {})", number_repr(value.x), number_repr(value.y),
					                       number_repr(value.z));
				    }
			    },
			    change);
		}

		std::string value_repr(ia::slot_value const& value) {
			return std::visit(
			    [](auto const& item) -> std::string {
				    using T = std::remove_cvref_t<decltype(item)>;
				    if constexpr (std::same_as<T, bool>) {
					    return item ? "True" : "False";
				    } else if constexpr (std::same_as<T, double>) {
					    return number_repr(item);
				    } else {
					    return unit_repr(item);
				    }
			    },
			    value);
		}

		// the keys of a map, sorted, so the repr does not depend on the
		// order of an unordered_map
		template <typename Map>
		std::vector<typename Map::key_type> sorted_keys(Map const& map) {
			std::vector<typename Map::key_type> keys{};
			keys.reserve(map.size());
			for (auto const& [key, _] : map)
				keys.push_back(key);
			std::ranges::sort(keys);
			return keys;
		}

		std::string state_repr(ia::state const& self) {
			auto result = std::format("state({})", str_repr(self.name));
			for (auto const& group : sorted_keys(self.groups)) {
				auto const& changes = self.groups.at(group);
				if (changes.size() == 1) {
					result += std::format(".transform({}, {})", str_repr(group), change_repr(changes.front()));
					continue;
				}
				std::vector<std::string> items{};
				for (auto const& change : changes)
					items.push_back(change_repr(change));
				result += std::format(".transform({}, {})", str_repr(group), list_repr(items));
			}
			for (auto const& slot : sorted_keys(self.slots)) {
				for (auto const& change : self.slots.at(slot))
					result += std::format(".set({}, {})", str_repr(slot), value_repr(change.set));
			}
			return result;
		}

		std::string transitions_repr(ia::transitions const& self) {
			std::string result{"transitions()"};
			if (self.defaults) result += std::format(".for_all({})", str_repr(*self.defaults));
			for (auto const& group : sorted_keys(self.groups))
				result += std::format(".for_group({}, {})", str_repr(group), str_repr(self.groups.at(group)));
			for (auto const& slot : sorted_keys(self.slots))
				result += std::format(".for_slot({}, {})", str_repr(slot), str_repr(self.slots.at(slot)));
			return result;
		}

		std::string slot_type_repr(ia::slot_type type) {
			switch (type) {
				case ia::slot_type::factor:
					return "slot_type.factor";
				case ia::slot_type::boolean:
					return "slot_type.boolean";
				case ia::slot_type::angle:
					return "slot_type.angle";
				case ia::slot_type::length:
					return "slot_type.length";
				case ia::slot_type::offset:
					return "slot_type.offset";
				case ia::slot_type::floating:
					break;
			}
			return "slot_type.float";
		}

		std::string direction_repr(ia::direction direction) {
			switch (direction) {
				case ia::direction::output:
					return "slot_direction.output";
				case ia::direction::variable:
					return "slot_direction.variable";
				case ia::direction::input:
					break;
			}
			return "slot_direction.input";
		}

		std::string slot_repr(ia::slot const& self) {
			auto result = std::format("slot(name={}, type={}, direction={}", str_repr(self.name),
			                          slot_type_repr(self.type), direction_repr(self.direction));
			if (self.default_value) result += std::format(", default={}", value_repr(*self.default_value));
			return result + ")";
		}

		std::string action_repr(ia::action const& self) {
			std::string when{};
			if (self.when.size() == 1) {
				when = str_repr(self.when.front());
			} else {
				std::vector<std::string> items{};
				for (auto const& name : self.when)
					items.push_back(str_repr(name));
				when = list_repr(items);
			}
			return std::format("action(label={}, when={}, then={})", str_repr(self.label), when, str_repr(self.then));
		}

		template <typename Item, typename Repr>
		std::string items_repr(std::vector<Item> const& items, Repr const& repr) {
			std::vector<std::string> result{};
			result.reserve(items.size());
			for (auto const& item : items)
				result.push_back(repr(item));
			return list_repr(result);
		}

		std::string geo_repr(ia::geo_active_object const& self) {
			return std::format("geo_active_object(name={}, slots={}, states={}, actions={}, transitions={})",
			                   str_repr(self.name), items_repr(self.slots, slot_repr),
			                   items_repr(self.states, state_repr), items_repr(self.actions, action_repr),
			                   transitions_repr(self.transitions));
		}

		std::string light_object_repr(ia::light_active_object const& self) {
			return std::format("light_active_object(name={}, light={})", str_repr(self.name), str_repr(self.light));
		}

		// __eq__ of a mutable wrapper: both values are copied under their
		// critical sections; pybind11 returns NotImplemented for any other
		// type, so Python falls back to the identity
		template <typename T>
		bool locked_eq(T const& self, T const& other) {
			return copy_of(self) == copy_of(other);
		}

		// __eq__ of an immutable one
		template <typename T, typename Class>
		void def_value_eq(Class& cls) {
			cls.def("__eq__", [](T const& self, T const& other) { return self == other; }, py::is_operator());
		}

		double finite_factor(double value, char const* what) { return finite(value, what); }

		void bind_channels(py::module_& m) {
			auto translate_axis =
			    py::class_<ia::translate_axis>(m, "translate_axis",
			                                   "Change of `translate.x`, `translate.y` or `translate.z`; made by "
			                                   "`channel.translate_x` and its siblings.")
			        .def_property_readonly("on", copy_member(&ia::translate_axis::on))
			        .def_property_readonly("by", copy_member(&ia::translate_axis::by))
			        .def("__repr__", [](ia::translate_axis const& self) { return change_repr(self); });
			def_value_eq<ia::translate_axis>(translate_axis);

			auto translate_all =
			    py::class_<ia::translate_all>(m, "translate_all", "Change of `translate`; made by `channel.translate`.")
			        .def_property_readonly("by", copy_member(&ia::translate_all::by))
			        .def("__repr__", [](ia::translate_all const& self) { return change_repr(self); });
			def_value_eq<ia::translate_all>(translate_all);

			auto rotate_axis =
			    py::class_<ia::rotate_axis>(
			        m, "rotate_axis",
			        "Change of `rotate.x`, `rotate.y` or `rotate.z`; made by `channel.rotate_x` and its siblings.")
			        .def_property_readonly("around", copy_member(&ia::rotate_axis::around))
			        .def_property_readonly("by", copy_member(&ia::rotate_axis::by))
			        .def("__repr__", [](ia::rotate_axis const& self) { return change_repr(self); });
			def_value_eq<ia::rotate_axis>(rotate_axis);

			auto rotate_all =
			    py::class_<ia::rotate_all>(m, "rotate_all",
			                               "Change of `rotate`, around X, then Y, then Z; made by `channel.rotate`.")
			        .def_property_readonly("x", copy_member(&ia::rotate_all::x))
			        .def_property_readonly("y", copy_member(&ia::rotate_all::y))
			        .def_property_readonly("z", copy_member(&ia::rotate_all::z))
			        .def("__repr__", [](ia::rotate_all const& self) { return change_repr(self); });
			def_value_eq<ia::rotate_all>(rotate_all);

			auto scale_axis =
			    py::class_<ia::scale_axis>(
			        m, "scale_axis",
			        "Change of `scale.x`, `scale.y` or `scale.z`; made by `channel.scale_x` and its siblings.")
			        .def_property_readonly("along", copy_member(&ia::scale_axis::along))
			        .def_property_readonly("by", copy_member(&ia::scale_axis::by))
			        .def("__repr__", [](ia::scale_axis const& self) { return change_repr(self); });
			def_value_eq<ia::scale_axis>(scale_axis);

			auto scale_all = py::class_<ia::scale_all>(m, "scale_all", "Change of `scale`; made by `channel.scale`.")
			                     .def_property_readonly("x", copy_member(&ia::scale_all::x))
			                     .def_property_readonly("y", copy_member(&ia::scale_all::y))
			                     .def_property_readonly("z", copy_member(&ia::scale_all::z))
			                     .def("__repr__", [](ia::scale_all const& self) { return change_repr(self); });
			def_value_eq<ia::scale_all>(scale_all);

			auto const zero_length = [](char const* name) { return py::arg_v(name, length{}, "cm(0)"); };
			auto const zero_angle = [](char const* name) { return py::arg_v(name, angle{}, "deg(0)"); };

			py::class_<ia::channel>(
			    m, "channel",
			    "Factories of the changes of a group, one per channel, for `state.transform`. The vector channels "
			    "take one to three values, the missing ones taking the default value of the channel.")
			    .def_static("translate_x", &ia::channel::translate_x, "by"_a)
			    .def_static("translate_y", &ia::channel::translate_y, "by"_a)
			    .def_static("translate_z", &ia::channel::translate_z, "by"_a)
			    .def_static("translate", py::overload_cast<length, length, length>(&ia::channel::translate), "x"_a,
			                zero_length("y"), zero_length("z"))
			    .def_static("translate", py::overload_cast<offset const&>(&ia::channel::translate), "by"_a)
			    .def_static("rotate_x", &ia::channel::rotate_x, "by"_a)
			    .def_static("rotate_y", &ia::channel::rotate_y, "by"_a)
			    .def_static("rotate_z", &ia::channel::rotate_z, "by"_a)
			    .def_static("rotate", &ia::channel::rotate, "x"_a, zero_angle("y"), zero_angle("z"),
			                "Rotates around X, then Y, then Z.")
			    .def_static(
			        "scale_x", [](double by) { return ia::channel::scale_x(finite_factor(by, "channel.scale_x: by")); },
			        "by"_a)
			    .def_static(
			        "scale_y", [](double by) { return ia::channel::scale_y(finite_factor(by, "channel.scale_y: by")); },
			        "by"_a)
			    .def_static(
			        "scale_z", [](double by) { return ia::channel::scale_z(finite_factor(by, "channel.scale_z: by")); },
			        "by"_a)
			    .def_static(
			        "scale",
			        [](double x, double y, double z) {
				        return ia::channel::scale(finite_factor(x, "channel.scale: x"),
				                                  finite_factor(y, "channel.scale: y"),
				                                  finite_factor(z, "channel.scale: z"));
			        },
			        "x"_a, "y"_a = 1.0, "z"_a = 1.0);
		}

		void bind_slots(py::module_& m) {
			py::native_enum<ia::slot_type>(m, "slot_type", "enum.Enum", "Type of the value of a slot.")
			    .value("float", ia::slot_type::floating)
			    .value("factor", ia::slot_type::factor)
			    .value("boolean", ia::slot_type::boolean)
			    .value("angle", ia::slot_type::angle)
			    .value("length", ia::slot_type::length)
			    .value("offset", ia::slot_type::offset)
			    .finalize();

			py::native_enum<ia::direction>(m, "slot_direction", "enum.Enum",
			                               "Direction of a slot; a variable is both an input and an output.")
			    .value("input", ia::direction::input)
			    .value("output", ia::direction::output)
			    .value("variable", ia::direction::variable)
			    .finalize();

			// immutable: nothing to lock
			auto slot = py::class_<ia::slot>(
			                m, "slot",
			                "Typed value an active object exposes to other models. The `default` is a bool, a "
			                "float, an angle, a length or an offset; None takes the default value of the type.")
			                .def(py::init([](std::string name, ia::slot_type type, ia::direction direction,
			                                 py::object const& default_value) {
				                     std::optional<ia::slot_value> value{};
				                     if (!default_value.is_none())
					                     value = to_slot_value(default_value, "slot: default");
				                     return ia::slot{std::move(name), type, direction, std::move(value)};
			                     }),
			                     "name"_a, "type"_a, "direction"_a, "default"_a = py::none())
			                .def_property_readonly("name", copy_member(&ia::slot::name))
			                .def_property_readonly("type", copy_member(&ia::slot::type))
			                .def_property_readonly("direction", copy_member(&ia::slot::direction))
			                .def_property_readonly("default_value", copy_member(&ia::slot::default_value))
			                .def("__repr__", &slot_repr);
			def_value_eq<ia::slot>(slot);

			auto slot_transformation =
			    py::class_<ia::slot_transformation>(m, "slot_transformation",
			                                        "Change of a slot in a state; made by `state.set`.")
			        .def_property_readonly("set", copy_member(&ia::slot_transformation::set))
			        .def("__repr__", [](ia::slot_transformation const& self) {
				        return std::format("slot_transformation(set={})", value_repr(self.set));
			        });
			def_value_eq<ia::slot_transformation>(slot_transformation);
		}

		// the builders return the wrapper they were called on
		constexpr auto ref = py::return_value_policy::reference;

		void bind_state(py::module_& m) {
			// mutable, and possibly shared by threads building models at
			// the same time: every access goes through locked.hpp
			py::class_<ia::state>(
			    m, "state",
			    "Named set of changes of groups and slots. Each call adds to the state and returns it, so the calls "
			    "can be chained, e.g. `state(\"opened\").transform(\"hinge\", channel.rotate_y(deg(80)))`.")
			    .def(py::init<std::string>(), "name"_a)
			    .def_property_readonly("name", [](ia::state const& self) { return copy_of(self).name; })
			    .def_property_readonly(
			        "groups", [](ia::state const& self) { return copy_of(self).groups; },
			        "A copy of the changes, by the name of the group, in the order they were added.")
			    .def_property_readonly(
			        "slots", [](ia::state const& self) { return copy_of(self).slots; },
			        "A copy of the changes, by the name of the slot, in the order they were added.")
			    .def(
			        "transform",
			        [](ia::state& self, std::string const& group,
			           ia::group_transformation const& change) -> ia::state& {
				        return glue::change(self, [&](ia::state& value) { value.transform(group, change); });
			        },
			        "group"_a, ref, "change"_a, ref,
			        "Appends a change, made by `channel`, to the list of the group; returns the state.")
			    .def(
			        "transform",
			        [](ia::state& self, std::string const& group,
			           std::vector<ia::group_transformation> const& changes) -> ia::state& {
				        return glue::change(self, [&](ia::state& value) { value.transform(group, changes); });
			        },
			        "group"_a, ref, "changes"_a, ref,
			        "Appends the changes, in their order, to the list of the group; returns the state.")
			    .def(
			        "set",
			        [](ia::state& self, std::string const& slot, py::object const& value) -> ia::state& {
				        auto const converted = to_slot_value(value, "state.set: value");
				        return glue::change(self, [&](ia::state& state) { state.set(slot, converted); });
			        },
			        "slot"_a, ref, "value"_a, ref,
			        "Sets an output slot or a variable, by its declared name, to a bool, a float, an angle, a length "
			        "or an offset; returns the state.")
			    .def("__eq__", &locked_eq<ia::state>, py::is_operator())
			    .def("__repr__", [](ia::state const& self) { return state_repr(copy_of(self)); });
		}

		void bind_action(py::module_& m) {
			// immutable: nothing to lock
			auto action =
			    py::class_<ia::action>(m, "action",
			                           "Labelled request to change the state: available in the states named by "
			                           "`when`, a single name or a list of them, it leads to the state `then`.")
			        .def(py::init([](std::string label, std::string when, std::string then) {
				             return ia::action{std::move(label), std::vector<std::string>{std::move(when)},
				                               std::move(then)};
			             }),
			             "label"_a, "when"_a, "then"_a)
			        .def(py::init<std::string, std::vector<std::string>, std::string>(), "label"_a, "when"_a, "then"_a)
			        .def_property_readonly("label", copy_member(&ia::action::label))
			        .def_property_readonly("when", copy_member(&ia::action::when))
			        .def_property_readonly("then", copy_member(&ia::action::then))
			        .def("__repr__", &action_repr);
			def_value_eq<ia::action>(action);
		}

		void bind_transitions(py::module_& m) {
			// mutable, as the state is
			auto const builder = [](auto&& call) {
				return [call](ia::transitions& self, std::string target, std::string rule) -> ia::transitions& {
					return glue::change(
					    self, [&](ia::transitions& value) { call(value, std::move(target), std::move(rule)); });
				};
			};

			py::class_<ia::transitions>(
			    m, "transitions",
			    "How the changes of an active object unfold, as CSS-like transition values, e.g. "
			    "`\"rotate.y 200ms ease-in\"`. Each call returns the transitions, so the calls can be chained; "
			    "calling one again for the same target replaces the earlier rule.")
			    .def(py::init<>())
			    .def_property_readonly(
			        "defaults", [](ia::transitions const& self) { return copy_of(self).defaults; },
			        "The rule for every target of the object, if any.")
			    .def_property_readonly(
			        "groups", [](ia::transitions const& self) { return copy_of(self).groups; },
			        "A copy of the rules, by the name of the group.")
			    .def_property_readonly(
			        "slots", [](ia::transitions const& self) { return copy_of(self).slots; },
			        "A copy of the rules, by the name of the slot.")
			    .def(
			        "for_all",
			        [](ia::transitions& self, std::string rule) -> ia::transitions& {
				        return glue::change(self, [&](ia::transitions& value) { value.for_all(std::move(rule)); });
			        },
			        "rule"_a, ref, "Sets the rule for every target of the object; returns the transitions.")
			    .def("for_group", builder([](ia::transitions& value, std::string group, std::string rule) {
				         value.for_group(std::move(group), std::move(rule));
			         }),
			         "group"_a, ref, "rule"_a, ref,
			         "Sets the rule of a group of the model, by its name; returns the transitions.")
			    .def("for_slot", builder([](ia::transitions& value, std::string slot, std::string rule) {
				         value.for_slot(std::move(slot), std::move(rule));
			         }),
			         "slot"_a, ref, "rule"_a, ref,
			         "Sets the rule of an output slot or a variable, by its declared name; returns the transitions.")
			    .def("__eq__", &locked_eq<ia::transitions>, py::is_operator())
			    .def("__repr__", [](ia::transitions const& self) { return transitions_repr(copy_of(self)); });
		}

		void bind_objects(py::module_& m) {
			// copies of the declarations, returned by `group.active_objects`;
			// immutable, as their parts are copies too
			auto geo =
			    py::class_<ia::geo_active_object>(m, "geo_active_object",
			                                      "Declaration of a geometry-based active object, as returned by "
			                                      "`group.active_objects`; declare one with `group.add_active_object`.")
			        .def_property_readonly("name", copy_member(&ia::geo_active_object::name))
			        .def_property_readonly("slots", copy_member(&ia::geo_active_object::slots))
			        .def_property_readonly("states", copy_member(&ia::geo_active_object::states))
			        .def_property_readonly("actions", copy_member(&ia::geo_active_object::actions))
			        .def_property_readonly("transitions", copy_member(&ia::geo_active_object::transitions))
			        .def("__repr__", &geo_repr);
			def_value_eq<ia::geo_active_object>(geo);

			auto light =
			    py::class_<ia::light_active_object>(
			        m, "light_active_object",
			        "Declaration of the transition of a light, as returned by `group.active_objects`; declare one "
			        "with `group.set_light_transition`.")
			        .def_property_readonly("name", copy_member(&ia::light_active_object::name))
			        .def_property_readonly("light", copy_member(&ia::light_active_object::light))
			        .def("__repr__", &light_object_repr);
			def_value_eq<ia::light_active_object>(light);
		}
	}  // namespace

	void bind_interactions(py::module_& m) {
		bind_channels(m);
		bind_slots(m);
		bind_state(m);
		bind_action(m);
		bind_transitions(m);
		bind_objects(m);
	}
}  // namespace shapes::glue
