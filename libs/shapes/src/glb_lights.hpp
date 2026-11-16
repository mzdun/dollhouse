// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <assimp/Exporter.hpp>

// Assimp's glTF 2 exporter writes no lights. The `glb2-lights` exporter runs
// it (as `glb2`) into memory, and adds KHR_lights_punctual to the binary,
// before writing it out.
//
// The node of every light is tagged with metadata (`light_key`: the index of
// the light in the scene; `range_key`: its range, if any), which the glTF 2
// exporter writes into the `extras` of the node. The tags are read back from
// there, and removed. The area lights have no place in KHR_lights_punctual,
// so they are left out.
namespace shapes::glb_lights {
	inline constexpr auto format_id = "glb2-lights";
	inline constexpr auto base_format_id = "glb2";

	inline constexpr auto light_key = "pyxel_light";
	// the key Assimp's glTF 2 importer puts the range under
	inline constexpr auto range_key = "PBR_LightRange";

	// registers `glb2-lights` with the exporter; it is not listed by
	// export_formats(), as it takes the place of `glb2`
	void register_exporter(Assimp::Exporter& exporter);
}  // namespace shapes::glb_lights
