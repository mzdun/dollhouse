# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Lights in the exported model: a light with no transformation of its own
takes the node of its named group, any other gets a generated node."""

import xml.etree.ElementTree as ET
from pathlib import Path

import pytest
import shapes
from shapes import I, cm, deg, solid

NS = {"c": "http://www.collada.org/2005/11/COLLADASchema"}


def export_dae(tmp_path: Path, fill) -> ET.Element:
    def lights_model(root: shapes.group):
        fill(root)
        return "collada"

    shapes.library.model(lights_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    return ET.parse(path).getroot()


def scene_root(dae: ET.Element) -> ET.Element:
    return dae.find("c:library_visual_scenes/c:visual_scene", NS)


def children(node: ET.Element) -> list[ET.Element]:
    return node.findall("c:node", NS)


def named(node: ET.Element, name: str) -> list[ET.Element]:
    return [child for child in children(node) if child.get("name") == name]


def all_named(dae: ET.Element, name: str) -> list[ET.Element]:
    """Nodes of the name, at any depth."""
    return [node for node in scene_root(dae).iter(f"{{{NS['c']}}}node") if node.get("name") == name]


def lights(dae: ET.Element) -> dict[str, ET.Element]:
    return {light.get("name"): light for light in dae.iterfind("c:library_lights/c:light", NS)}


def has_light(node: ET.Element) -> bool:
    return node.find("c:instance_light", NS) is not None


def translation(node: ET.Element) -> list[float]:
    m = [float(value) for value in node.find("c:matrix", NS).text.split()]
    return [m[3], m[7], m[11]]


def minus_z(node: ET.Element) -> list[float]:
    m = [float(value) for value in node.find("c:matrix", NS).text.split()]
    return [-m[2], -m[6], -m[10]]


def cube() -> solid:
    return solid.cube(cm(10), cm(10), cm(10))


def point() -> shapes.light:
    return shapes.light(type=shapes.light_type.point)


class TestNodes:
    def test_light_takes_its_named_group(self, tmp_path):
        def fill(root):
            root.add(cube())
            root.add_group(name="lamp", pos=shapes.at_y(cm(200))).add(point())

        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"lamp"}
        (lamp,) = all_named(dae, "lamp")
        assert has_light(lamp)
        assert translation(lamp) == pytest.approx([0, 2, 0])
        assert children(lamp) == []

    def test_group_with_meshes_gives_the_light_a_node(self, tmp_path):
        # Collada and FBX give a node either meshes or a light
        def fill(root):
            lamp = root.add_group(name="lamp")
            lamp.add(cube())
            lamp.add(point())

        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"Light"}
        (lamp,) = all_named(dae, "lamp")
        assert not has_light(lamp)
        assert has_light(named(lamp, "Light")[0])

    def test_light_only_group_is_kept(self, tmp_path):
        dae = export_dae(tmp_path, lambda root: root.add_group(name="lamp").add(point()))
        (lamp,) = named(scene_root(dae), "lamp")
        assert has_light(lamp)

    def test_transformed_light_gets_a_generated_node(self, tmp_path):
        def fill(root):
            lamp = root.add_group(name="lamp")
            lamp.add_group(pos=shapes.at_y(cm(50))).add(point())

        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"Light"}
        (lamp,) = named(scene_root(dae), "lamp")
        assert not has_light(lamp)
        (leaf,) = named(lamp, "Light")
        assert has_light(leaf)
        assert translation(leaf) == pytest.approx([0, 0.5, 0])

    def test_light_at_the_root_gets_a_generated_node(self, tmp_path):
        def fill(root):
            root.add(cube())
            root.add(point())

        # with meshes, the root is written as a node of its own, so the
        # generated node is one level down
        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"Light"}
        (leaf,) = all_named(dae, "Light")
        assert has_light(leaf)

    def test_second_light_in_a_group_gets_a_generated_node(self, tmp_path):
        def fill(root):
            lamp = root.add_group(name="lamp")
            lamp.add(point())
            lamp.add(point())

        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"lamp", "Light"}
        (lamp,) = named(scene_root(dae), "lamp")
        assert has_light(lamp)
        assert has_light(named(lamp, "Light")[0])

    def test_shared_group_names_get_generated_nodes(self, tmp_path):
        def fill(root):
            root.add_group(name="leg", pos=shapes.at_x(cm(50))).add(point())
            root.add_group(name="leg", pos=shapes.at_x(cm(-50))).add(point())

        dae = export_dae(tmp_path, fill)
        assert set(lights(dae)) == {"Light", "Light.001"}
        for leg in named(scene_root(dae), "leg"):
            assert not has_light(leg)
            assert len(children(leg)) == 1

    def test_generated_names_avoid_group_names(self, tmp_path):
        def fill(root):
            root.add_group(name="Light").add(cube())
            root.add(point())

        assert set(lights(export_dae(tmp_path, fill))) == {"Light.001"}

    def test_spot_shines_along_the_rotated_minus_z(self, tmp_path):
        def fill(root):
            grp = root.add_group(name="ceiling")
            # rotate -z (forward) to -y (down)
            grp.add_group().transform(I.rotate_x(deg(-90))).add(shapes.light(type=shapes.light_type.spot))

        (ceiling,) = named(scene_root(export_dae(tmp_path, fill)), "ceiling")
        (leaf,) = named(ceiling, "Light")
        assert minus_z(leaf) == pytest.approx([0, -1, 0], abs=1e-6)


class TestLights:
    def test_types(self, tmp_path):
        def fill(root):
            root.add_group(name="point").add(point())
            root.add_group(name="spot").add(shapes.light(type=shapes.light_type.spot))
            root.add_group(name="sun").add(shapes.light(type=shapes.light_type.directional))

        found = lights(export_dae(tmp_path, fill))
        assert found["point"].find(".//c:point", NS) is not None
        assert found["spot"].find(".//c:spot", NS) is not None
        assert found["sun"].find(".//c:directional", NS) is not None

    def test_intensity_is_folded_into_the_color(self, tmp_path):
        def fill(root):
            root.add_group(name="bulb").add(
                shapes.light(type=shapes.light_type.point, color=shapes.rgb(255, 0, 0), intensity=2)
            )

        point_el = lights(export_dae(tmp_path, fill))["bulb"].find(".//c:point", NS)
        assert [float(v) for v in point_el.find("c:color", NS).text.split()] == pytest.approx([2, 0, 0])

    def test_inverse_square_falloff(self, tmp_path):
        point_el = lights(export_dae(tmp_path, lambda root: root.add_group(name="bulb").add(point())))["bulb"].find(
            ".//c:point", NS
        )
        assert float(point_el.find("c:constant_attenuation", NS).text) == 0
        assert float(point_el.find("c:linear_attenuation", NS).text) == 0
        assert float(point_el.find("c:quadratic_attenuation", NS).text) == 1

    def test_spot_cone(self, tmp_path):
        def fill(root):
            root.add_group(name="spot").add(
                shapes.light(
                    type=shapes.light_type.spot,
                    cone=shapes.light_cone(inner_angle=deg(10), outer_angle=deg(20)),
                )
            )

        spot = lights(export_dae(tmp_path, fill))["spot"].find(".//c:spot", NS)
        # Collada's exporter writes the inner angle as the falloff angle
        assert float(spot.find("c:falloff_angle", NS).text) == pytest.approx(10, abs=1e-4)

    def test_area_light_does_not_break_the_export(self, tmp_path):
        def fill(root):
            root.add(cube())
            root.add_group(name="panel").add(shapes.light(type=shapes.light_type.area))

        dae = export_dae(tmp_path, fill)
        assert "panel" in lights(dae)
