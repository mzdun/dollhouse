// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <concepts>
#include <optional>
#include <pyxel/gfx/space.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// The declarations of active objects, as described in
// docs/model-interactions.md. They are plain values, built on the units of
// this library and on the standard library only; turning them into the JSON
// of the PYXEL_interactions extra and back is left to pyxel-io. Nothing here
// checks the rules needing the whole object or model, such as declared
// states or names of groups: those checks wait for the export.
namespace pyxel::gfx::interactions {
	// angle: radians; length: meters; in memory and in JSON alike

	using state_name = std::string;
	using group_name = std::string;  // a node of the model; no leading "$"
	using slot_name = std::string;   // as declared, without the added "$"

	// ---- group channels --------------------------------------------------

	// One alternative per kind of channel. The vector channels (translate,
	// rotate, scale) stay apart from the single-axis ones, as they are
	// separate channels for the transitions; their missing items are
	// filled with the default value of the channel.
	struct translate_axis {  // translate.x, translate.y, translate.z
		axis on;
		length by{};

		bool operator==(translate_axis const&) const = default;
	};

	struct translate_all {  // translate
		offset by;

		bool operator==(translate_all const&) const = default;
	};

	struct rotate_axis {  // rotate.x, rotate.y, rotate.z
		axis around;
		angle by{};

		bool operator==(rotate_axis const&) const = default;
	};

	struct rotate_all {  // rotate, applied around X, then Y, then Z
		angle x{}, y{}, z{};

		bool operator==(rotate_all const&) const = default;
	};

	struct scale_axis {  // scale.x, scale.y, scale.z
		axis along;
		double by{1};

		bool operator==(scale_axis const&) const = default;
	};

	struct scale_all {  // scale
		double x{1}, y{1}, z{1};

		bool operator==(scale_all const&) const = default;
	};

	using group_transformation =
	    std::variant<translate_axis, translate_all, rotate_axis, rotate_all, scale_axis, scale_all>;

	// Factories, one per channel. The vector channels take one to three
	// values, the missing ones taking the default value of the channel, as
	// in the JSON.
	struct channel {
		static translate_axis translate_x(length by) noexcept { return {.on = axis::x, .by = by}; }
		static translate_axis translate_y(length by) noexcept { return {.on = axis::y, .by = by}; }
		static translate_axis translate_z(length by) noexcept { return {.on = axis::z, .by = by}; }
		static translate_all translate(length x, length y = {}, length z = {}) noexcept {
			return {.by = {.x = x, .y = y, .z = z}};
		}
		static translate_all translate(offset const& by) noexcept { return {.by = by}; }

		static rotate_axis rotate_x(angle by) noexcept { return {.around = axis::x, .by = by}; }
		static rotate_axis rotate_y(angle by) noexcept { return {.around = axis::y, .by = by}; }
		static rotate_axis rotate_z(angle by) noexcept { return {.around = axis::z, .by = by}; }
		static rotate_all rotate(angle x, angle y = {}, angle z = {}) noexcept { return {.x = x, .y = y, .z = z}; }

		static scale_axis scale_x(double by) noexcept { return {.along = axis::x, .by = by}; }
		static scale_axis scale_y(double by) noexcept { return {.along = axis::y, .by = by}; }
		static scale_axis scale_z(double by) noexcept { return {.along = axis::z, .by = by}; }
		static scale_all scale(double x, double y = 1, double z = 1) noexcept { return {.x = x, .y = y, .z = z}; }
	};

	// ---- slots -----------------------------------------------------------

	enum class slot_type { floating, factor, boolean, angle, length, offset };  // floating is "float"
	enum class direction { input, output, variable };

	// double for float and factor
	using slot_value = std::variant<bool, double, angle, length, offset>;

	struct slot {
		slot_name name;
		slot_type type;
		interactions::direction direction;
		// "default" in JSON; std::nullopt for the default-initialized value
		// of the type
		std::optional<slot_value> default_value{};

		slot(slot_name name,
		     slot_type type,
		     interactions::direction direction,
		     std::optional<slot_value> default_value = std::nullopt)
		    : name{std::move(name)}, type{type}, direction{direction}, default_value{std::move(default_value)} {}

		bool operator==(slot const&) const = default;
	};

	struct slot_transformation {
		slot_value set;

		bool operator==(slot_transformation const&) const = default;
	};

	// ---- states ----------------------------------------------------------

	// One entry of the JSON mapping: the name, and the value split by the
	// "$" prefix of its keys. Each builder call adds to the state and
	// returns it, so the calls can be chained.
	struct state {
		state_name name;
		std::unordered_map<group_name, std::vector<group_transformation>> groups{};
		std::unordered_map<slot_name, std::vector<slot_transformation>> slots{};

		explicit state(state_name name) : name{std::move(name)} {}

		// Appends the changes to the list of `group`, keeping their order.
		// NOLINTNEXTLINE(build/include_what_you_use)
		state& transform(group_name const& group, group_transformation const& change);
		// NOLINTNEXTLINE(build/include_what_you_use)
		state& transform(group_name const& group, std::span<group_transformation const> changes);

		// Sets the output slot or variable `slot`, by its declared name,
		// without the "$" the JSON adds.
		state& set(slot_name const& slot, slot_value const& value);

		bool operator==(state const&) const = default;
	};

	// In the order of the JSON, as the first state may be the starting one.
	// A list, as the standard library has no map keeping the order of
	// insertion; an object has a handful of states, so a linear lookup by
	// name is enough.
	using states = std::vector<state>;

	// ---- actions ---------------------------------------------------------

	struct action {
		std::string label;
		std::vector<state_name> when;  // a single name is read as a list of one
		state_name then;

		action(std::string label, std::vector<state_name> when, state_name then)
		    : label{std::move(label)}, when{std::move(when)}, then{std::move(then)} {}
		// A template, so that a braced list, such as {"tilted", "opened"},
		// goes to the overload above: as a plain state_name, it could also
		// become a std::string built from two iterators.
		template <std::convertible_to<state_name> Name>
		action(std::string label, Name&& when, state_name then)
		    : action(std::move(label), std::vector<state_name>{state_name(std::forward<Name>(when))}, std::move(then)) {
		}

		bool operator==(action const&) const = default;
	};

	// ---- transitions -----------------------------------------------------

	// a comma-separated list of single-channel transitions
	using transition = std::string;

	// The JSON mapping, split like a state: "*" on its own, the other keys
	// by their "$" prefix into groups and slots, the slots kept under their
	// declared names. Each builder call returns the transitions, so the
	// calls can be chained; calling one again for the same target replaces
	// the earlier rule.
	struct transitions {
		std::optional<transition> defaults{};  // "*" in JSON
		std::unordered_map<group_name, transition> groups{};
		std::unordered_map<slot_name, transition> slots{};

		// The defaults for every target of the object, "*" in the JSON.
		transitions& for_all(transition rule);

		// The transition of a group of the model, by its name.
		transitions& for_group(group_name group, transition rule);

		// The transition of an output slot or a variable, by its declared
		// name, without the "$" the JSON adds.
		transitions& for_slot(slot_name slot, transition rule);

		bool operator==(transitions const&) const = default;
	};

	// ---- active objects --------------------------------------------------

	struct light_active_object {
		std::string name;    // the name of a light in the same model
		transition light{};  // "transitions": {"light": ...} in JSON

		bool operator==(light_active_object const&) const = default;
	};

	struct geo_active_object {
		std::string name;
		std::vector<slot> slots{};
		interactions::states states{};
		std::vector<action> actions{};
		interactions::transitions transitions{};

		bool operator==(geo_active_object const&) const = default;
	};

	using active_object = std::variant<geo_active_object, light_active_object>;
	using active_objects = std::vector<active_object>;
}  // namespace pyxel::gfx::interactions
