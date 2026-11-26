// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <memory>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/io/interactions.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace pyxel::io {
	// a file the model was read from, or would be read from, if it was there
	struct source_file {
		// absolute and normalized
		std::filesystem::path path;
		// false: looked for and not found; once it shows up, the model
		// read again would change
		bool exists{true};

		bool operator==(source_file const&) const = default;
	};

	struct imported_model {
		gfx::loaded_model model;
		// what the importer did to the active objects, other than reading
		// them as they are: the ones it dropped, by a rule or by their
		// version, and the ones it upgraded or read in part, for the
		// viewer to tell the user (see The reading side in
		// docs/model-interactions.md)
		std::vector<interactions::note> notes;
		// the files to watch for changes of the model: the model itself
		// first, then each file Assimp opened or looked for (the buffers of
		// a `.gltf`, the material library of an `.obj`) and the external
		// textures of the materials, which Assimp names, but does not
		// open; each file once, in the order of first use
		std::vector<source_file> sources;
	};

	// Thrown by import_model(), with the files it read or looked for before
	// it failed: the one that is missing, broken or half-written. The model
	// read again, after one of them changes or shows up, may come out
	// right, so a watcher keeps these until it does, next to the files of
	// the last good import.
	class import_error : public std::runtime_error {
	public:
		import_error(std::string const& message, std::vector<source_file> sources);

		std::vector<source_file> const& sources() const noexcept { return *sources_; }

	private:
		// shared, so a copy of the exception does not throw
		std::shared_ptr<std::vector<source_file> const> sources_;
	};

	// Reads a model in any format Assimp reads, with the importer of ours
	// for glTF 2, which adds the area lights and the active objects. The
	// faces are split into triangles; nothing merges or removes the nodes
	// (see Keeping the named nodes in the specification). FBX files are
	// converted to meters; the other formats with no unit of their own are
	// taken to be in meters. A model in a format other than glTF 2 has no
	// active objects of its own, but each named light is still a light
	// source. Throws import_error, if the file cannot be read.
	imported_model import_model(std::filesystem::path const& path);
}  // namespace pyxel::io
