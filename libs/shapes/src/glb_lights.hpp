// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <assimp/Exporter.hpp>

// Assimp's glTF 2 exporter writes no lights. The `glb2-lights` exporter runs
// it (as `glb2`) into memory, and adds KHR_lights_punctual to the binary,
// before writing it out.
//
// The node of every light is tagged with metadata (`light_key`: the index of
// the light in the scene; `range_key`: its range, if any; for area lights,
// `area_shape_key` and `area_spread_key`), which the glTF 2 exporter writes
// into the `extras` of the node. The tags are read back from there, and
// removed.
//
// The area lights have no place in KHR_lights_punctual; they go to an
// extension of ours, PYXEL_lights_area, laid out the same way: a `lights`
// array in the extensions of the document, and `{"light": index}` in the
// extensions of the node. It is listed in `extensionsUsed`, but not in
// `extensionsRequired`, so viewers not knowing it still load the file,
// without the area lights. Every light there has:
//
// - `name`, `color` and `intensity` (in candela), as in KHR_lights_punctual
// - `shape`: "rectangle" or "ellipse"; squares and disks are written as
//   rectangles and ellipses of equal sides
// - `width` and `height`, in metres: the size of the emitting surface (for
//   ellipses, the diameters) along the local x and y; it faces -z
// - `spread`: the full angle of the emission around -z, in radians, up to pi
//   (the whole front half-space)
// - `range`, optional, as in KHR_lights_punctual
namespace shapes::glb_lights {
	inline constexpr auto format_id = "glb2-lights";
	inline constexpr auto base_format_id = "glb2";

	inline constexpr auto light_key = "pyxel_light";
	// the key Assimp's glTF 2 importer puts the range under
	inline constexpr auto range_key = "PBR_LightRange";
	// area lights only; the shape is one of the `area_*` values below, the
	// spread is in radians
	inline constexpr auto area_shape_key = "pyxel_area_shape";
	inline constexpr auto area_spread_key = "pyxel_area_spread";

	inline constexpr int area_rectangle = 0;
	inline constexpr int area_ellipse = 1;

	// registers `glb2-lights` with the exporter; it is not listed by
	// export_formats(), as it takes the place of `glb2`
	void register_exporter(Assimp::Exporter& exporter);
}  // namespace shapes::glb_lights
