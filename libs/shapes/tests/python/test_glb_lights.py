# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Lights in glTF 2 binaries: `glb2` is exported through `glb2-lights`, which
adds KHR_lights_punctual to what Assimp writes."""

import json
import math
import struct
from pathlib import Path
from typing import Any, Callable

import pytest
import shapes
from shapes import I, cm, deg, solid

EXTENSION = "KHR_lights_punctual"
GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A


def read_glb(path: Path) -> tuple[dict[str, Any], list[tuple[int, bytes]]]:
    """The JSON of a GLB file, and all its chunks, as (type, data)."""
    data = path.read_bytes()
    magic, version, total = struct.unpack_from("<III", data, 0)
    assert magic == GLB_MAGIC
    assert version == 2
    assert total == len(data)

    chunks: list[tuple[int, bytes]] = []
    offset = 12
    while offset < total:
        length, kind = struct.unpack_from("<II", data, offset)
        assert length % 4 == 0, "chunks are aligned to 4 bytes"
        chunks.append((kind, data[offset + 8 : offset + 8 + length]))
        offset += 8 + length
    assert offset == total
    assert chunks[0][0] == CHUNK_JSON
    return json.loads(chunks[0][1]), chunks


def export_glb(tmp_path: Path, fill: Callable[[shapes.group], None]) -> dict[str, Any]:
    def lights_model(root: shapes.group) -> None:
        fill(root)
        # None: the default exporter, glb2

    shapes.library.model(lights_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    assert path.suffix == ".glb"
    doc, _ = read_glb(path)
    return doc


def lights(doc: dict[str, Any]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = doc["extensions"][EXTENSION]["lights"]
    return result


def nodes_with_lights(doc: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Lights by the name of their node."""
    found = lights(doc)
    return {
        node.get("name", ""): found[node["extensions"][EXTENSION]["light"]]
        for node in doc["nodes"]
        if EXTENSION in node.get("extensions", {})
    }


def cube() -> solid:
    return solid.cube(cm(10), cm(10), cm(10))


def spot(**kwargs: Any) -> shapes.light:
    return shapes.light(type=shapes.light_type.spot, **kwargs)


class TestFormat:
    def test_glb2_lights_is_not_listed(self):
        assert "glb2" in shapes.library.exporters
        assert "glb2-lights" not in shapes.library.exporters

    def test_no_lights_no_extension(self, tmp_path):
        doc = export_glb(tmp_path, lambda root: root.add(cube()))
        assert EXTENSION not in doc.get("extensionsUsed", [])
        assert EXTENSION not in doc.get("extensions", {})

    def test_meshes_survive(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add(cube())
            root.add_group(name="lamp").add(spot())

        doc = export_glb(tmp_path, fill)
        assert len(doc["meshes"]) == 1
        assert doc["buffers"][0]["byteLength"] > 0


class TestLights:
    def test_extension_is_used(self, tmp_path):
        doc = export_glb(tmp_path, lambda root: root.add_group(name="lamp").add(spot()))
        assert EXTENSION in doc["extensionsUsed"]
        assert set(nodes_with_lights(doc)) == {"lamp"}

    def test_names_keep_spaces(self, tmp_path):
        doc = export_glb(
            tmp_path, lambda root: root.add_group(name="hob light").add(spot())
        )
        assert set(nodes_with_lights(doc)) == {"hob light"}
        assert lights(doc)[0]["name"] == "hob light"

    def test_tags_are_removed(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add_group(name="lamp").add(spot(range=cm(300)))
            root.add_group(pos=shapes.at_x(cm(10))).add(spot())

        doc = export_glb(tmp_path, fill)
        text = json.dumps(doc)
        assert "pyxel_light" not in text
        assert "PBR_LightRange" not in text

    def test_types(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add_group(name="point").add(shapes.light(type=shapes.light_type.point))
            root.add_group(name="spot").add(spot())
            root.add_group(name="sun").add(
                shapes.light(type=shapes.light_type.directional)
            )

        found = nodes_with_lights(export_glb(tmp_path, fill))
        assert {name: light["type"] for name, light in found.items()} == {
            "point": "point",
            "spot": "spot",
            "sun": "directional",
        }

    def test_area_lights_are_left_out(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add(cube())
            root.add_group(name="panel").add(shapes.light(type=shapes.light_type.area))

        doc = export_glb(tmp_path, fill)
        assert EXTENSION not in doc.get("extensionsUsed", [])
        assert "pyxel_light" not in json.dumps(doc)

    def test_color_and_intensity(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add_group(name="red").add(
                spot(color=shapes.rgb(255, 0, 0), intensity=2)
            )
            root.add_group(name="white").add(spot(intensity=60))

        found = nodes_with_lights(export_glb(tmp_path, fill))
        assert found["red"]["color"] == pytest.approx([1, 0, 0])
        assert found["red"]["intensity"] == pytest.approx(2)
        assert found["white"]["color"] == pytest.approx([1, 1, 1])
        assert found["white"]["intensity"] == pytest.approx(60)

    def test_range(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add_group(name="near").add(spot(range=cm(250)))
            root.add_group(name="far").add(spot())
            root.add_group(name="sun").add(
                shapes.light(type=shapes.light_type.directional, range=cm(250))
            )

        found = nodes_with_lights(export_glb(tmp_path, fill))
        assert found["near"]["range"] == pytest.approx(2.5)
        assert "range" not in found["far"]
        # meaningless for directional lights
        assert "range" not in found["sun"]

    def test_spot_cone(self, tmp_path):
        cone = shapes.light_cone(inner_angle=deg(10), outer_angle=deg(20))
        doc = export_glb(
            tmp_path, lambda root: root.add_group(name="lamp").add(spot(cone=cone))
        )
        angles = lights(doc)[0]["spot"]
        assert angles["innerConeAngle"] == pytest.approx(math.radians(10))
        assert angles["outerConeAngle"] == pytest.approx(math.radians(20))

    def test_hard_edge_stays_valid(self, tmp_path):
        cone = shapes.light_cone(inner_angle=deg(30), outer_angle=deg(30))
        doc = export_glb(
            tmp_path, lambda root: root.add_group(name="lamp").add(spot(cone=cone))
        )
        angles = lights(doc)[0]["spot"]
        # KHR_lights_punctual: 0 <= inner < outer <= 90 degrees
        assert angles["innerConeAngle"] < angles["outerConeAngle"]
        assert angles["outerConeAngle"] == pytest.approx(math.radians(30))

    def test_transformed_light_gets_its_own_node(self, tmp_path):
        def fill(root: shapes.group) -> None:
            lamp = root.add_group(name="lamp")
            lamp.add(cube())
            lamp.add_group().transform(I.rotate_x(-deg(90))).add(spot())

        doc = export_glb(tmp_path, fill)
        assert set(nodes_with_lights(doc)) == {"Light"}
        (light_node,) = [node for node in doc["nodes"] if node.get("name") == "Light"]
        (lamp,) = [node for node in doc["nodes"] if node.get("name") == "lamp"]
        assert doc["nodes"].index(light_node) in lamp["children"]
        # glTF spots shine along -z of their node: here, down
        m = light_node["matrix"]
        assert [-m[8], -m[9], -m[10]] == pytest.approx([0, -1, 0], abs=1e-6)


class TestOtherFormats:
    def test_collada_has_no_tags(self, tmp_path):
        def lights_model(root: shapes.group) -> str:
            root.add_group(name="lamp").add(spot(range=cm(300)))
            return "collada"

        shapes.library.model(lights_model)
        shapes.library.output_directory = tmp_path
        try:
            (path,) = shapes.library.build_models()
        finally:
            shapes.library.clear_models()
        text = path.read_text(encoding="UTF-8")
        assert "pyxel_light" not in text
        assert "PBR_LightRange" not in text
