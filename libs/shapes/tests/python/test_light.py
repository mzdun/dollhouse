# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

import math

import pytest
import shapes
from shapes import cm, deg

# a light sits at the origin of its local space and shines along -z


def ahead(distance: shapes.length) -> shapes.offset:
    return shapes.at_z(-distance)


class TestLight:
    def test_defaults(self):
        lamp = shapes.light()
        assert lamp.type == shapes.light_type.point
        assert lamp.name == ""
        assert lamp.color == shapes.rgb(255, 255, 255)
        assert lamp.intensity == 1
        assert lamp.range is None
        # as in KHR_lights_punctual
        assert lamp.cone == shapes.light_cone(inner_angle=deg(0), outer_angle=deg(45))
        assert lamp == shapes.light()

    def test_no_placement(self):
        lamp = shapes.light()
        for name in ("position", "rot", "spot"):
            assert not hasattr(lamp, name)
        with pytest.raises(TypeError):
            shapes.light(position=shapes.offset())

    def test_keywords(self):
        lamp = shapes.light(
            type=shapes.light_type.spot,
            name="desk",
            color=shapes.rgb(255, 200, 150),
            intensity=8,
            range=cm(500),
            cone=shapes.light_cone(inner_angle=deg(10), outer_angle=deg(20)),
        )
        assert lamp.type == shapes.light_type.spot
        assert lamp.name == "desk"
        assert lamp.color == shapes.rgb(255, 200, 150)
        assert lamp.range == cm(500)
        window = (1 - (200 / 500) ** 4) ** 2
        assert lamp.illuminance_at(ahead(cm(200))) == pytest.approx(8 / 4 * window)
        # 45 degrees off the axis
        assert lamp.illuminance_at(shapes.offset(x=cm(200), z=cm(-200))) == 0

    def test_positional_args_rejected(self):
        with pytest.raises(TypeError):
            shapes.light(shapes.light_type.point)

    def test_inverse_square_falloff(self):
        lamp = shapes.light(intensity=100)
        assert lamp.falloff_at(cm(100)) == pytest.approx(1)
        assert lamp.falloff_at(cm(200)) == pytest.approx(0.25)
        assert math.isinf(lamp.falloff_at(cm(0)))
        # a point light shines in every direction
        assert lamp.illuminance_at(shapes.at_x(cm(500))) == pytest.approx(4)
        assert lamp.illuminance_at(shapes.at_z(cm(500))) == pytest.approx(4)

    def test_range(self):
        lamp = shapes.light(range=cm(200))
        assert lamp.falloff_at(cm(200)) == 0
        lamp.range = None
        assert lamp.falloff_at(cm(200)) == pytest.approx(0.25)

    @pytest.mark.parametrize("intensity", [-1, math.inf, math.nan])
    def test_invalid_intensity(self, intensity):
        with pytest.raises(ValueError, match="intensity"):
            shapes.light(intensity=intensity)
        lamp = shapes.light()
        with pytest.raises(ValueError, match="intensity"):
            lamp.intensity = intensity
        assert lamp.intensity == 1

    @pytest.mark.parametrize("value", [cm(0), cm(-1)])
    def test_invalid_range(self, value):
        with pytest.raises(ValueError, match="range"):
            shapes.light(range=value)
        lamp = shapes.light()
        with pytest.raises(ValueError, match="range"):
            lamp.range = value
        assert lamp.range is None

    def test_directional_ignores_distance(self):
        sun = shapes.light(type=shapes.light_type.directional, intensity=1000, range=cm(1))
        assert sun.illuminance_at(shapes.at_x(cm(10000))) == pytest.approx(1000)

    def test_cone_is_squared(self):
        cone = shapes.light_cone(inner_angle=deg(10), outer_angle=deg(20))
        cos_in, cos_out = math.cos(math.radians(10)), math.cos(math.radians(20))
        linear = (math.cos(math.radians(15)) - cos_out) / (cos_in - cos_out)
        assert cone.at(deg(15)) == pytest.approx(linear**2)

    def test_spot_shines_along_minus_z(self):
        lamp = shapes.light(type=shapes.light_type.spot, intensity=4)
        assert lamp.illuminance_at(ahead(cm(200))) == pytest.approx(1)
        assert lamp.illuminance_at(shapes.at_z(cm(200))) == 0

    def test_nested_properties_modify_the_light(self):
        lamp = shapes.light()
        lamp.cone.outer_angle = deg(60)
        lamp.cone.inner_angle = deg(30)

        assert lamp.cone.outer_angle.to_deg() == pytest.approx(60)
        assert lamp.cone.inner_angle.to_deg() == pytest.approx(30)
        assert lamp != shapes.light()

    def test_direct_assignment(self):
        lamp = shapes.light()
        lamp.type = shapes.light_type.spot
        lamp.name = "bulb"
        lamp.color = shapes.rgb(10, 20, 30)
        lamp.intensity = 4
        lamp.cone = shapes.light_cone(outer_angle=deg(30))
        assert lamp.type == shapes.light_type.spot
        assert lamp.name == "bulb"
        assert lamp.color == shapes.rgb(10, 20, 30)
        assert lamp.cone.outer_angle.to_deg() == pytest.approx(30)
        assert lamp.illuminance_at(ahead(cm(200))) == pytest.approx(1)

    def test_repr(self):
        text = repr(shapes.light(type=shapes.light_type.spot, name="bulb", range=cm(300)))
        assert text == (
            "light(type=light_type.spot, name='bulb', color=rgb(255, 255, 255), intensity=1, range=cm(300), "
            "cone=light_cone(inner_angle=deg(0), outer_angle=deg(45)), "
            "area=light_area(shape=area_shape.square, width=cm(100), height=cm(100), spread=deg(180)))"
        )
        assert "range=None" in repr(shapes.light())


class TestLightArea:
    def test_defaults(self):
        area = shapes.light().area
        assert area.shape == shapes.area_shape.square
        assert area.width == cm(100)
        assert area.height == cm(100)
        assert area.spread.to_deg() == pytest.approx(180)
        assert area == shapes.light_area()

    def test_keywords(self):
        area = shapes.light_area(shape=shapes.area_shape.rectangle, width=cm(200), height=cm(50), spread=deg(90))
        assert area.shape == shapes.area_shape.rectangle
        assert area.effective_height() == cm(50)
        assert area.surface().value == pytest.approx(1.0)

    @pytest.mark.parametrize(
        "shape, square_metres",
        [
            (shapes.area_shape.square, 4.0),
            (shapes.area_shape.rectangle, 6.0),
            (shapes.area_shape.disk, math.pi),
            (shapes.area_shape.ellipse, 1.5 * math.pi),
        ],
    )
    def test_surface(self, shape, square_metres):
        area = shapes.light_area(shape=shape, width=cm(200), height=cm(300))
        assert area.surface().value == pytest.approx(square_metres)

    def test_nested_properties_modify_the_light(self):
        lamp = shapes.light(type=shapes.light_type.area)
        lamp.area.shape = shapes.area_shape.disk
        lamp.area.width = cm(30)
        lamp.area.spread = deg(120)
        assert lamp.area == shapes.light_area(shape=shapes.area_shape.disk, width=cm(30), spread=deg(120))

    def test_spread_limits_illuminance(self):
        lamp = shapes.light(type=shapes.light_type.area, intensity=8)
        off_axis = shapes.offset(x=cm(200), z=cm(-200))
        # 45 degrees off the axis, at 2 * sqrt(2) m
        assert lamp.illuminance_at(off_axis) == pytest.approx(1)
        # behind the surface
        assert lamp.illuminance_at(shapes.at_z(cm(200))) == 0
        lamp.area.spread = deg(60)
        assert lamp.illuminance_at(off_axis) == 0

    def test_repr(self):
        text = repr(shapes.light_area(shape=shapes.area_shape.ellipse, width=cm(20), height=cm(10)))
        assert text == "light_area(shape=area_shape.ellipse, width=cm(20), height=cm(10), spread=deg(180))"
