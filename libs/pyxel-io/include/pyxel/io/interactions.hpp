// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/interactions.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The JSON of the PYXEL_interactions extra (see docs/model-interactions.md
// and schemas/interactions.schema.json). The interface takes and returns
// the text of the extra, so no JSON library shows in it.
namespace pyxel::io::interactions {
	// What parse() did to one active object, other than reading it as is.
	struct note {
		enum kind { upgraded, ignored, dropped };

		std::string object;  // the name of the active object, if it has one
		kind what;
		std::string reason;  // for the user, e.g. "version 2.0 is not supported"

		bool operator==(note const&) const = default;
	};

	struct parsed {
		gfx::interactions::active_objects objects;
		std::vector<note> notes;

		bool operator==(parsed const&) const = default;
	};

	// The names of the model's lights tell which entries describe lights.
	// Reads each object on its own, according to its "version" (see
	// Versions in the specification): an object of a newer minor version
	// loses what this version does not know, with an `ignored` note. An
	// object it cannot read, being of an unreadable version or not of the
	// shape of the data structures, is left out of `objects` and reported
	// in `notes`; the other objects are still read.
	// Throws std::runtime_error only for text that is not JSON, or JSON that
	// is not an array. The rules of this specification, such as declared
	// states or reserved names, are not checked here: both the exporter and
	// the importer check them, as only they know the whole model. Neither do
	// the transition values get parsed: they are kept as text.
	parsed parse(std::string_view json, std::span<std::string const> light_names);

	// The text of the PYXEL_interactions extra: a compact JSON array, with
	// "1.0" as the version of every object. The states keep their order;
	// the other mappings, unordered in the declarations, are written with
	// their keys sorted, so the same declarations give the same text. An
	// empty part of an object (no slots, no states, no actions, no
	// transitions, an empty light transition) is left out. Nothing is
	// checked here: the rules are the exporter's.
	std::string serialize(gfx::interactions::active_objects const& objects);
}  // namespace pyxel::io::interactions
