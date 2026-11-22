// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <pyxel/gfx/materials.hpp>
#include <pyxel/gfx/sink.hpp>
#include <string>
#include <vector>

namespace pyxel::io {
	struct export_format {
		std::string id{};
		std::string extension{};
		std::string description{};  // as given by Assimp; may be empty
	};

	// every export format of the linked Assimp
	std::vector<export_format> const& export_formats();

	// writes the nodes collected by `out` to `path`, using the given export
	// format; every node keeps its name and local transformation, and the
	// nodes with no meshes, in them or below, are left out, unless an active
	// object names them; the root node is unnamed, `name` is used in error
	// messages; throws std::runtime_error on failure.
	// Only the materials used by the solids are exported, looked up in
	// `repo`; solids with an unknown or empty material get a magenta one.
	// The active objects of the nodes, in the order of the tree, go to the
	// PYXEL_interactions extra of the root node; a model with any of them
	// can be exported to `glb2` and `gltf2` only (see
	// docs/model-interactions.md)
	void export_scene(std::string const& name,
	                  pyxel::gfx::sink const& out,
	                  pyxel::gfx::materials const& repo,
	                  std::string const& format_id,
	                  std::filesystem::path const& path);
}  // namespace pyxel::io
