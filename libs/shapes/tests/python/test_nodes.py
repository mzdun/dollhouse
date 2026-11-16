# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Named groups and named sink scopes become nodes of the exported model,
keeping their transformations; unnamed groups are baked into the vertices."""

import xml.etree.ElementTree as ET
from pathlib import Path

import pytest
import shapes
from shapes import I, cm, solid

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}

Vec = tuple[float, float, float]


def export_dae(tmp_path: Path, fill) -> ET.Element:
    def nodes_model(root: shapes.group):
        fill(root)
        return "collada"

    shapes.library.model(nodes_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    return ET.parse(path).getroot()


def find(element: ET.Element, path: str) -> ET.Element:
    """The element at `path`; fails the test, if there is none."""
    found = element.find(path, NS)
    assert found is not None, f"no {path} in <{element.tag}>"
    return found


def text(element: ET.Element, path: str) -> str:
    """Text of the element at `path`; fails the test, if there is none."""
    value = find(element, path).text
    assert value is not None, f"no text in {path}"
    return value


def floats(element: ET.Element, path: str) -> list[float]:
    return [float(value) for value in text(element, path).split()]


def scene_root(dae: ET.Element) -> ET.Element:
    return find(dae, "c:library_visual_scenes/c:visual_scene")


def children(node: ET.Element) -> list[ET.Element]:
    return node.findall("c:node", NS)


def named(node: ET.Element, name: str) -> list[ET.Element]:
    return [child for child in children(node) if child.get("name") == name]


def translation(node: ET.Element) -> list[float]:
    """Translation column of the node's row-major matrix, in metres."""
    m = floats(node, "c:matrix")
    return [m[3], m[7], m[11]]


def geometry_positions(dae: ET.Element, node: ET.Element) -> list[Vec]:
    """Positions of the first mesh instanced by the node, in the node's space."""
    url = find(node, "c:instance_geometry").get("url", "").lstrip("#")
    geometry = find(dae, f"c:library_geometries/c:geometry[@id='{url}']")
    values = floats(geometry, ".//c:float_array")
    return [
        (values[index], values[index + 1], values[index + 2])
        for index in range(0, len(values), 3)
    ]


def cube() -> solid:
    return solid.cube(cm(10), cm(10), cm(10))


class box(shapes.sized):
    def __init__(self, name: str = "", m=None):
        super().__init__(cm(10), cm(10), cm(10))
        self.name = name
        self.m = m

    def output(self, sink, m):
        with sink.scope(self.name, m=self.m):
            sink.add_solid(cube(), m)


class TestScope:
    def test_matrix_needs_a_name(self, tmp_path):
        errors = []

        class nameless(shapes.sized):
            def __init__(self):
                super().__init__(cm(1), cm(1), cm(1))

            def output(self, sink, m):
                try:
                    sink.scope(m=I.translate(cm(10)))
                except ValueError as exc:
                    errors.append(str(exc))
                sink.add_solid(cube(), m)

        export_dae(tmp_path, lambda root: root.add(nameless()))
        assert len(errors) == 1
        assert "needs an object_name" in errors[0]

    def test_named_scope_opens_a_node(self, tmp_path):
        dae = export_dae(
            tmp_path, lambda root: root.add(box("hinge", I.translate(cm(50))))
        )
        (hinge,) = named(scene_root(dae), "hinge")
        assert translation(hinge) == pytest.approx([0.5, 0, 0])
        # the vertices stay in the space of the node
        positions = geometry_positions(dae, hinge)
        assert min(p[0] for p in positions) == pytest.approx(0)
        assert max(p[0] for p in positions) == pytest.approx(0.1)


class TestGroups:
    def test_named_group_keeps_its_transformation(self, tmp_path):
        def fill(root):
            room = root.add_group(name="room", pos=shapes.at_x(cm(100)))
            room.add(cube())

        dae = export_dae(tmp_path, fill)
        (room,) = named(scene_root(dae), "room")
        assert translation(room) == pytest.approx([1, 0, 0])
        positions = geometry_positions(dae, room)
        assert min(p[0] for p in positions) == pytest.approx(0)

    def test_unnamed_groups_are_baked_into_the_nearest_node(self, tmp_path):
        def fill(root):
            room = root.add_group(name="room", pos=shapes.at_x(cm(100)))
            room.add_group(pos=shapes.at_y(cm(20))).add(cube())

        dae = export_dae(tmp_path, fill)
        (room,) = named(scene_root(dae), "room")
        assert children(room) == []
        positions = geometry_positions(dae, room)
        assert min(p[1] for p in positions) == pytest.approx(0.2)

    def test_unnamed_groups_compose_into_named_node(self, tmp_path):
        def fill(root):
            outer = root.add_group(pos=shapes.at_x(cm(100)))
            door = outer.add_group(name="door", pos=shapes.at_z(cm(30)))
            door.add(cube())

        (door,) = named(scene_root(export_dae(tmp_path, fill)), "door")
        assert translation(door) == pytest.approx([1, 0, 0.3])

    def test_same_names_are_separate_nodes(self, tmp_path):
        def fill(root):
            root.add_group(name="leg", pos=shapes.at_x(cm(50))).add(cube())
            root.add_group(name="leg", pos=shapes.at_x(cm(-50))).add(cube())

        legs = named(scene_root(export_dae(tmp_path, fill)), "leg")
        assert sorted(translation(leg)[0] for leg in legs) == pytest.approx([-0.5, 0.5])

    def test_empty_nodes_are_left_out(self, tmp_path):
        def fill(root):
            root.add_group(name="empty").add_group(name="nested")
            root.add_group(name="full").add(cube())

        names = [
            node.get("name")
            for node in children(scene_root(export_dae(tmp_path, fill)))
        ]
        assert "empty" not in names
        assert "full" in names

    def test_node_with_only_named_children_is_kept(self, tmp_path):
        def fill(root):
            house = root.add_group(name="house", pos=shapes.at_x(cm(100)))
            house.add_group(name="door", pos=shapes.at_x(cm(10))).add(cube())

        (house,) = named(scene_root(export_dae(tmp_path, fill)), "house")
        (door,) = named(house, "door")
        assert translation(house) == pytest.approx([1, 0, 0])
        assert translation(door) == pytest.approx([0.1, 0, 0])
