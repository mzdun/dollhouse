# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Lights in glTF 2 files: `glb2` and `gltf2` are exported through
`glb2-pyxel` and `gltf2-pyxel`, which add KHR_lights_punctual and
PYXEL_lights_area to what Assimp writes."""

import json
import math
import struct
from pathlib import Path
from typing import Any, Callable

import pytest
import shapes
from shapes import I, cm, deg, solid

EXTENSION = "KHR_lights_punctual"
AREA_EXTENSION = "PYXEL_lights_area"
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


Fill = Callable[[shapes.group], None]
Export = Callable[[Fill], dict[str, Any]]


def build(tmp_path: Path, fill: Fill, exporter: str | None) -> Path:
    def lights_model(root: shapes.group) -> str | None:
        fill(root)
        # None: the default exporter, glb2
        return exporter

    shapes.library.model(lights_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    return path


def export_glb(tmp_path: Path, fill: Fill) -> dict[str, Any]:
    path = build(tmp_path, fill, None)
    assert path.suffix == ".glb"
    doc, _ = read_glb(path)
    return doc


def export_gltf(tmp_path: Path, fill: Fill) -> dict[str, Any]:
    path = build(tmp_path, fill, "gltf2")
    assert path.suffix == ".gltf"
    doc: dict[str, Any] = json.loads(path.read_text(encoding="UTF-8"))
    return doc


@pytest.fixture(name="export_model", params=["glb2", "gltf2"])
def fixture_export_model(request, tmp_path) -> Export:
    """Exports the model to the format of the test and returns its JSON."""
    if request.param == "gltf2":
        return lambda fill: export_gltf(tmp_path, fill)
    return lambda fill: export_glb(tmp_path, fill)


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


def area_lights(doc: dict[str, Any]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = doc["extensions"][AREA_EXTENSION]["lights"]
    return result


def nodes_with_area_lights(doc: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """Area lights by the name of their node."""
    found = area_lights(doc)
    return {
        node.get("name", ""): found[node["extensions"][AREA_EXTENSION]["light"]]
        for node in doc["nodes"]
        if AREA_EXTENSION in node.get("extensions", {})
    }


def cube() -> solid:
    return solid.cube(cm(10), cm(10), cm(10))


def spot(**kwargs: Any) -> shapes.light:
    return shapes.light(type=shapes.light_type.spot, **kwargs)


def area(**kwargs: Any) -> shapes.light:
    return shapes.light(type=shapes.light_type.area, **kwargs)


class TestFormat:
    def test_lights_exporters_are_not_listed(self):
        assert "glb2" in shapes.library.exporters
        assert "gltf2" in shapes.library.exporters
        assert "glb2-pyxel" not in shapes.library.exporters
        assert "gltf2-pyxel" not in shapes.library.exporters

    def test_no_lights_no_extension(self, export_model):
        doc = export_model(lambda root: root.add(cube()))
        assert EXTENSION not in doc.get("extensionsUsed", [])
        assert EXTENSION not in doc.get("extensions", {})

    def test_meshes_survive(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add(cube())
            root.add_group(name="lamp").add(spot())

        doc = export_model(fill)
        assert len(doc["meshes"]) == 1
        assert doc["buffers"][0]["byteLength"] > 0

    def test_gltf_buffer_is_next_to_it(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add(cube())
            root.add_group(name="lamp").add(spot())

        path = build(tmp_path, fill, "gltf2")
        doc = json.loads(path.read_text(encoding="UTF-8"))
        (buffer,) = doc["buffers"]
        assert "/" not in buffer["uri"]
        assert (path.parent / buffer["uri"]).stat().st_size == buffer["byteLength"]
        assert EXTENSION in doc["extensionsUsed"]


class TestLights:
    def test_extension_is_used(self, export_model):
        doc = export_model(lambda root: root.add_group(name="lamp").add(spot()))
        assert EXTENSION in doc["extensionsUsed"]
        assert set(nodes_with_lights(doc)) == {"lamp"}

    def test_names_keep_spaces(self, export_model):
        doc = export_model(lambda root: root.add_group(name="hob light").add(spot()))
        assert set(nodes_with_lights(doc)) == {"hob light"}
        assert lights(doc)[0]["name"] == "hob light"

    def test_tags_are_removed(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="lamp").add(spot(range=cm(300)))
            root.add_group(pos=shapes.at_x(cm(10))).add(spot())

        doc = export_model(fill)
        text = json.dumps(doc)
        assert "pyxel_light" not in text
        assert "PBR_LightRange" not in text

    def test_types(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="point").add(shapes.light(type=shapes.light_type.point))
            root.add_group(name="spot").add(spot())
            root.add_group(name="sun").add(
                shapes.light(type=shapes.light_type.directional)
            )

        found = nodes_with_lights(export_model(fill))
        assert {name: light["type"] for name, light in found.items()} == {
            "point": "point",
            "spot": "spot",
            "sun": "directional",
        }

    def test_area_lights_are_not_punctual(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add(cube())
            root.add_group(name="panel").add(area())

        doc = export_model(fill)
        assert EXTENSION not in doc.get("extensionsUsed", [])
        assert EXTENSION not in doc.get("extensions", {})

    def test_color_and_intensity(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="red").add(
                spot(color=shapes.rgb(255, 0, 0), intensity=2)
            )
            root.add_group(name="white").add(spot(intensity=60))

        found = nodes_with_lights(export_model(fill))
        assert found["red"]["color"] == pytest.approx([1, 0, 0])
        assert found["red"]["intensity"] == pytest.approx(2)
        assert found["white"]["color"] == pytest.approx([1, 1, 1])
        assert found["white"]["intensity"] == pytest.approx(60)

    def test_range(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="near").add(spot(range=cm(250)))
            root.add_group(name="far").add(spot())
            root.add_group(name="sun").add(
                shapes.light(type=shapes.light_type.directional, range=cm(250))
            )

        found = nodes_with_lights(export_model(fill))
        assert found["near"]["range"] == pytest.approx(2.5)
        assert "range" not in found["far"]
        # meaningless for directional lights
        assert "range" not in found["sun"]

    def test_spot_cone(self, export_model):
        cone = shapes.light_cone(inner_angle=deg(10), outer_angle=deg(20))
        doc = export_model(
            lambda root: root.add_group(name="lamp").add(spot(cone=cone))
        )
        angles = lights(doc)[0]["spot"]
        assert angles["innerConeAngle"] == pytest.approx(math.radians(10))
        assert angles["outerConeAngle"] == pytest.approx(math.radians(20))

    def test_hard_edge_stays_valid(self, export_model):
        cone = shapes.light_cone(inner_angle=deg(30), outer_angle=deg(30))
        doc = export_model(
            lambda root: root.add_group(name="lamp").add(spot(cone=cone))
        )
        angles = lights(doc)[0]["spot"]
        # KHR_lights_punctual: 0 <= inner < outer <= 90 degrees
        assert angles["innerConeAngle"] < angles["outerConeAngle"]
        assert angles["outerConeAngle"] == pytest.approx(math.radians(30))

    def test_transformed_light_gets_its_own_node(self, export_model):
        def fill(root: shapes.group) -> None:
            lamp = root.add_group(name="lamp")
            lamp.add(cube())
            lamp.add_group().transform(I.rotate_x(-deg(90))).add(spot())

        doc = export_model(fill)
        assert set(nodes_with_lights(doc)) == {"Light"}
        (light_node,) = [node for node in doc["nodes"] if node.get("name") == "Light"]
        (lamp,) = [node for node in doc["nodes"] if node.get("name") == "lamp"]
        assert doc["nodes"].index(light_node) in lamp["children"]
        # glTF spots shine along -z of their node: here, down
        m = light_node["matrix"]
        assert [-m[8], -m[9], -m[10]] == pytest.approx([0, -1, 0], abs=1e-6)


class TestAreaLights:
    def test_extension_is_used_not_required(self, export_model):
        doc = export_model(lambda root: root.add_group(name="panel").add(area()))
        assert AREA_EXTENSION in doc["extensionsUsed"]
        assert AREA_EXTENSION not in doc.get("extensionsRequired", [])
        assert set(nodes_with_area_lights(doc)) == {"panel"}

    def test_no_area_lights_no_extension(self, export_model):
        doc = export_model(lambda root: root.add_group(name="lamp").add(spot()))
        assert AREA_EXTENSION not in doc.get("extensionsUsed", [])
        assert AREA_EXTENSION not in doc.get("extensions", {})

    def test_mixed_with_punctual(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="lamp").add(spot())
            root.add_group(name="panel").add(area())
            root.add_group(name="bulb").add(shapes.light(type=shapes.light_type.point))

        doc = export_model(fill)
        assert set(nodes_with_lights(doc)) == {"lamp", "bulb"}
        assert set(nodes_with_area_lights(doc)) == {"panel"}
        assert {EXTENSION, AREA_EXTENSION} <= set(doc["extensionsUsed"])

    def test_tags_are_removed(self, export_model):
        def fill(root: shapes.group) -> None:
            root.add_group(name="panel").add(area(range=cm(300)))
            root.add_group(pos=shapes.at_x(cm(10))).add(area())

        doc = export_model(fill)
        text = json.dumps(doc)
        for tag in (
            "pyxel_light",
            "PBR_LightRange",
            "pyxel_area_shape",
            "pyxel_area_spread",
        ):
            assert tag not in text
        assert all("extras" not in node for node in doc["nodes"])

    @pytest.mark.parametrize(
        "shape, width, height, expected",
        [
            (shapes.area_shape.square, 120, 40, ("rectangle", 1.2, 1.2)),
            (shapes.area_shape.rectangle, 120, 40, ("rectangle", 1.2, 0.4)),
            (shapes.area_shape.disk, 30, 50, ("ellipse", 0.3, 0.3)),
            (shapes.area_shape.ellipse, 30, 50, ("ellipse", 0.3, 0.5)),
        ],
    )
    def test_shapes(self, export_model, shape, width, height, expected):
        surface = shapes.light_area(shape=shape, width=cm(width), height=cm(height))
        doc = export_model(
            lambda root: root.add_group(name="panel").add(area(area=surface))
        )
        (light,) = area_lights(doc)
        kind, w, h = expected
        assert light["shape"] == kind
        assert light["width"] == pytest.approx(w)
        assert light["height"] == pytest.approx(h)

    def test_light_properties(self, export_model):
        surface = shapes.light_area(spread=deg(120))

        def fill(root: shapes.group) -> None:
            root.add_group(name="warm").add(
                area(
                    color=shapes.rgb(255, 0, 0),
                    intensity=300,
                    range=cm(250),
                    area=surface,
                )
            )
            root.add_group(name="plain").add(area())

        found = nodes_with_area_lights(export_model(fill))
        warm, plain = found["warm"], found["plain"]
        assert warm["name"] == "warm"
        assert warm["color"] == pytest.approx([1, 0, 0])
        assert warm["intensity"] == pytest.approx(300)
        assert warm["range"] == pytest.approx(2.5)
        assert warm["spread"] == pytest.approx(math.radians(120))
        assert plain["color"] == pytest.approx([1, 1, 1])
        assert plain["intensity"] == pytest.approx(1)
        assert plain["spread"] == pytest.approx(math.pi)
        assert "range" not in plain
        assert "type" not in plain

    def test_transformed_light_gets_its_own_node(self, export_model):
        def fill(root: shapes.group) -> None:
            lamp = root.add_group(name="lamp")
            lamp.add(cube())
            lamp.add_group().transform(I.rotate_x(-deg(90))).add(area())

        doc = export_model(fill)
        assert set(nodes_with_area_lights(doc)) == {"Light"}


class TestOtherFormats:
    def test_collada_has_no_tags(self, tmp_path):
        def fill(root: shapes.group) -> None:
            root.add_group(name="lamp").add(spot(range=cm(300)))
            root.add_group(name="panel").add(area())

        path = build(tmp_path, fill, "collada")
        text = path.read_text(encoding="UTF-8")
        for tag in (
            "pyxel_light",
            "PBR_LightRange",
            "pyxel_area_shape",
            "pyxel_area_spread",
        ):
            assert tag not in text
