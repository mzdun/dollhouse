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
}  // namespace pyxel::io::interactions
