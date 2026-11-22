// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <pyxel/gfx/interactions.hpp>
#include <string>
#include <string_view>
#include <vector>

// The rules of docs/model-interactions.md (see Rules there), checked on the
// whole model. The exporter fails on any problem found; the importer is to
// drop the objects they name.
namespace pyxel::io::interactions {
	// What the rules need to know about the model.
	struct model_names {
		// the names of the nodes, with the number of nodes of each name; the
		// empty name is not counted
		std::map<std::string, size_t, std::less<>> nodes{};
		// the names of the lights, with the number of lights of each name;
		// a light with an empty name is not an active object, and is not
		// counted
		std::map<std::string, size_t, std::less<>> lights{};

		void add_node(std::string const& name) {
			if (!name.empty()) ++nodes[name];
		}
		void add_light(std::string const& name) {
			if (!name.empty()) ++lights[name];
		}
	};

	struct problem {
		// the index of the faulty active object; empty for a problem of the
		// model itself, such as two lights of the same name
		std::optional<size_t> object;
		std::string reason;

		bool operator==(problem const&) const = default;
	};

	// Every problem found, in the order of the objects; the problems of the
	// model come first. An object describing a light, which shares its name
	// with another light, or which another object describes, is named in a
	// problem of its own.
	std::vector<problem> check(gfx::interactions::active_objects const& objects, model_names const& model);

	// ---- transitions -----------------------------------------------------

	enum class easing { ease, ease_in, ease_out, ease_in_out, linear };

	// One item of a transition value, with the omitted parts filled from
	// `all 0s ease 0s`; a negative time is taken as 0s.
	struct single_transition {
		std::string channel{"all"};
		interactions::easing easing{easing::ease};
		double duration{};  // seconds
		double delay{};     // seconds

		bool operator==(single_transition const&) const = default;
	};

	struct transition_result {
		// empty for `none`
		std::vector<single_transition> items{};
		// empty, if the value parses
		std::string error{};
	};

	// The grammar of Transitions grammar in the specification; keywords and
	// units are ASCII case-insensitive, the channel names come back in
	// lowercase. The channels are not matched with any target here.
	transition_result parse_transition(std::string_view value);
}  // namespace pyxel::io::interactions
