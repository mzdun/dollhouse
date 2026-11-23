// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/io/interactions.hpp>
#include <vector>

namespace pyxel::io {
	struct imported_model {
		gfx::loaded_model model;
		// what the importer did to the active objects, other than reading
		// them as they are: the ones it dropped, by a rule or by their
		// version, and the ones it upgraded or read in part, for the
		// viewer to tell the user (see The reading side in
		// docs/model-interactions.md)
		std::vector<interactions::note> notes;
	};

	// Reads a model in any format Assimp reads, with the importer of ours
	// for glTF 2, which adds the area lights and the active objects. The
	// faces are split into triangles; nothing merges or removes the nodes
	// (see Keeping the named nodes in the specification). FBX files are
	// converted to meters; the other formats with no unit of their own are
	// taken to be in meters. A model in a format other than glTF 2 has no
	// active objects of its own, but each named light is still a light
	// source. Throws std::runtime_error, if the file cannot be read.
	imported_model import_model(std::filesystem::path const& path);
}  // namespace pyxel::io
