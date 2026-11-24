// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <pyxel/gfx/loaded_model.hpp>
#include <span>

namespace pyxel::io {
	// Writes a copy of the glTF 2 file `source` to `target`, posed: the
	// local matrix of every node of `model` (as import_model() read it from
	// `source`) is followed by its move, `local' = local · M`, one move per
	// node, as loaded_model::moves() gives them. Nothing else of the
	// document changes, but:
	//
	// - a moved node gets its local matrix as `matrix`, in place of
	//   `translation`, `rotation` and `scale`; its numbers are rounded to
	//   1e-9, so the same pose gives the same text everywhere;
	// - the PYXEL_interactions extras are left out, as the states would move
	//   the posed nodes once more; any glTF viewer shows the pose, and the
	//   copy has no active objects of its own;
	// - the relative URIs of the buffers and the images are rewritten for
	//   the directory of `target`, so a `.gltf` copy still finds its `.bin`
	//   files; they are taken as plain paths, as the exporters of ours
	//   write them.
	//
	// The moved nodes are found by name, as the active objects find them:
	// the first of the name, depth-first through the default scene. The copy
	// keeps the container of `source`, so `target` needs the extension of
	// it, `.glb` for a binary and `.gltf` for a text. Throws
	// std::runtime_error, if `source` cannot be read or is not glTF 2, if
	// the extension does not match, if a moved node cannot be found, or if
	// `target` cannot be written.
	void write_posed(std::filesystem::path const& source,
	                 std::filesystem::path const& target,
	                 gfx::loaded_model const& model,
	                 std::span<glm::dmat4 const> moves);
}  // namespace pyxel::io
