// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/transition.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What an importer holds for each active object once it has read the
// PYXEL_interactions extra and checked the rules: the declarations of
// pyxel/gfx/interactions.hpp, resolved for running them (see Actions and
// Instantiation in docs/model-interactions.md, and
// examples/interactions-loaded.yaml for the same objects as the ones
// in examples/interactions-stored.yaml). Nothing here is stored in a
// file; it is all derived from the declarations.
//
// States, groups and slots are referred to by their index in the lists of
// the loaded object; the names stay in the lists.
namespace pyxel::gfx::interactions {
	enum class object_kind { geometry, light };

	// ---- channels --------------------------------------------------------

	// The channels of a group (see Channels in the specification).
	enum class group_channel {
		translate_x,
		translate_y,
		translate_z,
		translate,
		rotate_x,
		rotate_y,
		rotate_z,
		rotate,
		scale_x,
		scale_y,
		scale_z,
		scale,
	};

	// The channel a transformation sets.
	group_channel channel_of(group_transformation const& change) noexcept;
	// The name of the channel in a transition value, such as "rotate.x".
	std::string_view name_of(group_channel channel) noexcept;
	// The transformation setting the channel to its default value: 0 for a
	// translation or a rotation, 1 for a scaling.
	group_transformation default_of(group_channel channel) noexcept;
	// The declared default value of the slot, or the default-initialized
	// value of its type.
	slot_value default_of(slot const& declared) noexcept;

	// The channels of one group any state of the object sets, in the order
	// they first appear, going through the states in declared order, with
	// the transition of each.
	struct group_channels {
		group_name group;
		std::vector<group_channel> channels{};
		std::vector<timing> transitions{};  // one per channel

		bool operator==(group_channels const&) const = default;
	};

	// An output slot or a variable some state of the object sets.
	struct slot_channel {
		size_t slot;  // into loaded_object::slots
		timing transition{};

		bool operator==(slot_channel const&) const = default;
	};

	// The value of every channel of the object, as the lists of
	// loaded_object::groups and loaded_object::slot_channels go: a group
	// channel holds the transformation setting it.
	struct channel_values {
		std::vector<std::vector<group_transformation>> groups{};
		std::vector<slot_value> slots{};

		bool operator==(channel_values const&) const = default;
	};

	// ---- states ----------------------------------------------------------

	// The index of the default interaction in loaded_state::machine.
	inline constexpr std::ptrdiff_t default_interaction = -1;

	struct loaded_state {
		state_name name;
		// The outgoing edges: an index into loaded_object::actions, or
		// default_interaction, to an index into loaded_object::states. A
		// state with no outgoing actions has no edges, not even the default
		// one.
		std::map<std::ptrdiff_t, size_t> machine{};
		// The groups that move between this state and any state its edges
		// lead to, as indices into loaded_object::groups, in their order.
		std::vector<size_t> highlights{};
		// The future value of every channel in this state, the ones the
		// state does not set filled with their default values.
		channel_values values{};

		bool operator==(loaded_state const&) const = default;
	};

	// ---- storage ---------------------------------------------------------

	// The template the storage of a placement starts from (see
	// Instantiation): settled in the starting state, with the previous, the
	// current and the future value of each channel all equal to `channels`.
	struct storage {
		std::optional<size_t> state{};  // into loaded_object::states
		double progress{1};
		// the slot channels are left empty here: their values are the ones
		// of their slots
		channel_values channels{};
		std::optional<double> light{};    // the intensity factor of a light source
		std::vector<slot_value> slots{};  // one per declared slot, inputs included

		bool operator==(storage const&) const = default;
	};

	// ---- active objects --------------------------------------------------

	struct loaded_object {
		std::string name;
		object_kind kind{object_kind::geometry};
		// as declared, or as synthesized for a light source; every default
		// value is filled in
		std::vector<slot> slots{};
		// the first state, if the object has any
		std::optional<size_t> start{};
		// the labels of the actions, in declared order
		std::vector<std::string> actions{};
		std::vector<loaded_state> states{};
		std::vector<group_channels> groups{};
		std::vector<slot_channel> slot_channels{};
		// the transition of the intensity factor of a light source
		timing light{};
		interactions::storage storage{};

		bool operator==(loaded_object const&) const = default;

		// The state of the name, if declared.
		std::optional<size_t> find_state(std::string_view state) const noexcept;
	};

	using loaded_objects = std::vector<loaded_object>;

	// The objects are expected to follow the rules marked for import (see
	// Rules in the specification), as the importer drops the ones that do
	// not. Should they still break one, no exception is thrown: what does
	// not fit, such as an action leading to an undeclared state, a slot
	// state for an undeclared slot or a transition value that does not
	// parse, is left out.
	loaded_object load(geo_active_object const& object);
	loaded_object load(light_active_object const& object);

	// Every active object, in declared order, followed by a light source for
	// every named light of the model no object describes, in the order of
	// `light_names`. A light with an empty name is not an active object.
	loaded_objects load(active_objects const& objects, std::span<std::string const> light_names);
}  // namespace pyxel::gfx::interactions
