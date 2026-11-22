// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/interactions.hpp>
#include <string>

// The JSON of the PYXEL_interactions extra (see docs/model-interactions.md
// and schemas/interactions.schema.json). The interface takes and returns
// the text of the extra, so no JSON library shows in it.
namespace pyxel::io::interactions {
	// The text of the PYXEL_interactions extra: a compact JSON array, with
	// "1.0" as the version of every object. The states keep their order;
	// the other mappings, unordered in the declarations, are written with
	// their keys sorted, so the same declarations give the same text. An
	// empty part of an object (no slots, no states, no actions, no
	// transitions, an empty light transition) is left out. Nothing is
	// checked here: the rules are the exporter's.
	std::string serialize(gfx::interactions::active_objects const& objects);
}  // namespace pyxel::io::interactions
