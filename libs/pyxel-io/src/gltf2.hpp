// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <pyxel/io/interactions.hpp>
#include <string_view>

// Assimp's glTF 2 exporter writes no lights. The `glb2-pyxel` and
// `gltf2-pyxel` exporters run it (as `glb2` and `gltf2`) into memory, add
// KHR_lights_punctual to its JSON document, and write the file out. For the
// binary, the JSON is the first chunk; for the text, it is the `.gltf` file,
// while the buffers go to their `.bin` files, as Assimp writes them.
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
// - `width` and `height`, in meters: the size of the emitting surface (for
//   ellipses, the diameters) along the local x and y; it faces -z
// - `spread`: the full angle of the emission around -z, in radians, up to pi
//   (the whole front half-space)
// - `range`, optional, as in KHR_lights_punctual
//
// The exporters also write the PYXEL_interactions extra (see
// docs/model-interactions.md): its text comes in the string property
// `interactions_property` of the export, and goes, parsed, to the `extras`
// of the root node of the scene. Assimp cannot carry it on a node, as it
// cuts metadata strings and flattens arrays. Without the property, or with
// an empty one, nothing is added.
//
// Assimp's glTF 2 importer reads KHR_lights_punctual, but not the area
// lights. The importer of ours takes its place: it runs it, then reads the
// JSON document again, and adds an aiLight of aiLightSource_AREA for every
// node with PYXEL_lights_area, the reverse of the exporters:
//
// - the light is named after its node, as Assimp names the punctual ones
// - the intensity is folded into the colors, as Assimp does it
// - the width and the height go to `mSize`
// - the node gets the metadata the exporters read: `area_shape_key` and
//   `area_spread_key`, and `range_key`, if there is a range
//
// An area light, which cannot be read, is left out with a warning in
// Assimp's log, as are the area lights of a node with a punctual light,
// whose range would use the same key.
//
// The importer also reads the PYXEL_interactions extra from the document
// itself, as Assimp cuts and flattens its copy in the metadata. It looks
// for it in every node, as another application may have wrapped the model
// in a new root, and merges the lists in the order of the `nodes` array.
// The names of the lights of the scene tell which objects describe lights.
// Every note of the parser goes to Assimp's log as a warning, and the
// result is kept by the importer, for interactions_of().
namespace pyxel::io::gltf2 {
	inline constexpr auto bin_format_id = "glb2-pyxel";
	inline constexpr auto bin_base_format_id = "glb2";
	inline constexpr auto text_format_id = "gltf2-pyxel";
	inline constexpr auto text_base_format_id = "gltf2";

	inline constexpr auto light_key = "pyxel_light";
	// the key Assimp's glTF 2 importer puts the range under
	inline constexpr auto range_key = "PBR_LightRange";
	// area lights only; the shape is one of the `area_*` values below, the
	// spread is in radians
	inline constexpr auto area_shape_key = "pyxel_area_shape";
	inline constexpr auto area_spread_key = "pyxel_area_spread";

	// the export property with the text of the PYXEL_interactions extra
	inline constexpr auto interactions_property = "pyxel_interactions";
	// the key of the extra in the `extras` of the root node
	inline constexpr auto interactions_extra = "PYXEL_interactions";
	// the key of the scene metadata, under which the importer of ours
	// stamps every scene it reads with a number of its own (uint64), to
	// tell it from a later scene at the same address
	inline constexpr auto import_stamp_key = "pyxel_import";

	inline constexpr int area_rectangle = 0;
	inline constexpr int area_ellipse = 1;

	// the format to export to, when `format_id` was asked for: `glb2-pyxel`
	// for `glb2`, `gltf2-pyxel` for `gltf2`, `format_id` itself for the
	// others
	std::string_view exporter_for(std::string_view format_id) noexcept;

	// registers `glb2-pyxel` and `gltf2-pyxel` with the exporter; they are
	// not listed by export_formats(), as they take the place of `glb2` and
	// `gltf2`
	void register_exporters(Assimp::Exporter& exporter);

	// the name of Assimp's glTF 2 importer, and of the one taking its place
	inline constexpr auto base_importer_name = "glTF2 Importer";
	inline constexpr auto importer_name = "glTF2 Importer, with PYXEL_lights_area";

	// replaces Assimp's glTF 2 importer with the one of ours, which reads
	// `.gltf` and `.glb` files, as it does, and adds the area lights; does
	// nothing, if there is no glTF 2 importer to replace (e.g. it was
	// replaced already)
	void register_importer(Assimp::Importer& importer);

	// The active objects of `scene`, as read by the importer of ours, with
	// the notes about the ones left out or read in part; nullptr, if the
	// importer of ours is not registered with `importer`, or if `scene` is
	// not the scene it read last (e.g. the file was not glTF 2, even if its
	// scene took the address of the freed glTF 2 one)
	interactions::parsed const* interactions_of(Assimp::Importer const& importer, aiScene const* scene);
}  // namespace pyxel::io::gltf2
