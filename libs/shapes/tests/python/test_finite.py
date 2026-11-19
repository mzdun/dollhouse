# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""NaN and infinity never get into the model: a NaN or infinite argument
raises ValueError, a division by zero ZeroDivisionError, and a result too
large for a float OverflowError, as with the floats of Python."""

import math

import pytest
import shapes
from shapes import cm, deg, mm, rad

NAN = math.nan
INF = math.inf
NOT_FINITE = [NAN, INF, -INF]
NOT_FINITE_IDS = ["nan", "inf", "-inf"]

# 1e308 m, as large as a length gets
HUGE = cm(1e300) * 1e10


@pytest.fixture(params=NOT_FINITE, ids=NOT_FINITE_IDS)
def bad(request) -> float:
    return request.param


class TestArguments:
    @pytest.mark.parametrize("make", [cm, mm, rad, deg])
    def test_units(self, make, bad):
        with pytest.raises(ValueError, match="must be finite"):
            make(bad)

    def test_measures(self, bad):
        with pytest.raises(ValueError, match="must be finite"):
            shapes.area(cm2=bad)
        with pytest.raises(ValueError, match="must be finite"):
            shapes.volume(mm3=bad)

    @pytest.mark.parametrize(
        "value",
        [cm(1), deg(1), shapes.area(cm2=1), shapes.volume(cm3=1), shapes.offset(x=cm(1))],
        ids=["length", "angle", "area", "volume", "offset"],
    )
    def test_factors_and_divisors(self, value, bad):
        with pytest.raises(ValueError, match="must be finite"):
            _ = value * bad
        with pytest.raises(ValueError, match="must be finite"):
            _ = bad * value
        with pytest.raises(ValueError, match="must be finite"):
            _ = value / bad

    def test_in_place_leaves_the_value(self, bad):
        point = shapes.offset(x=cm(1))
        with pytest.raises(ValueError):
            point *= bad
        assert point == shapes.offset(x=cm(1))

    def test_size_scaling(self, bad):
        box = shapes.size(cm(1), cm(1), cm(1))
        with pytest.raises(ValueError, match="must be finite"):
            box.scaled(y=bad)
        with pytest.raises(ValueError, match="must be finite"):
            _ = box * bad

    def test_object_scaling(self, bad):
        obj = shapes.sized(cm(1), cm(1), cm(1))
        with pytest.raises(ValueError, match="must be finite"):
            obj.scale(bad)
        with pytest.raises(ValueError, match="must be finite"):
            obj.scale(z=bad)
        assert obj.size == shapes.size(cm(1), cm(1), cm(1))

    def test_lerp(self, bad):
        with pytest.raises(ValueError, match="must be finite"):
            shapes.lerp(shapes.offset(), shapes.offset(x=cm(1)), bad)

    def test_matrix(self, bad):
        with pytest.raises(ValueError, match="must be finite"):
            shapes.I.scale(bad)
        with pytest.raises(ValueError, match="must be finite"):
            shapes.I.rotate(deg(90), bad, 0, 0)

    def test_spinner(self, bad):
        rng = shapes.RNG("0123456789abcdef0123456789abcdef")
        with pytest.raises(ValueError, match="must be finite"):
            rng.spinner(bad, 1)
        with pytest.raises(ValueError, match="must be finite"):
            rng.spinner(0, bad)

    def test_spinner_range_too_wide(self):
        rng = shapes.RNG("0123456789abcdef0123456789abcdef")
        with pytest.raises(ValueError, match="too wide"):
            rng.spinner(-1e308, 1e308)

    def test_paint(self, bad):
        with pytest.raises(ValueError, match="roughness"):
            shapes.library.register_paint(shapes.rgb(255, 0, 0), roughness=bad)
        with pytest.raises(ValueError, match="metallic"):
            shapes.library.register_paint(shapes.rgb(255, 0, 0), metallic=bad)


class TestDivisionByZero:
    @pytest.mark.parametrize(
        "value",
        [
            cm(1),
            deg(1),
            shapes.area(cm2=1),
            shapes.volume(cm3=1),
            shapes.offset(x=cm(1)),
            shapes.size(cm(1), cm(1), cm(1)),
        ],
        ids=["length", "angle", "area", "volume", "offset", "size"],
    )
    def test_raises_zero_division(self, value):
        with pytest.raises(ZeroDivisionError):
            _ = value / 0

    def test_in_place(self):
        point = shapes.offset(x=cm(1))
        with pytest.raises(ZeroDivisionError):
            point /= 0
        assert point == shapes.offset(x=cm(1))


class TestOverflow:
    def test_huge_is_finite(self):
        assert math.isfinite(HUGE.value)

    def test_product(self):
        with pytest.raises(OverflowError):
            _ = HUGE * 10

    def test_sum(self):
        with pytest.raises(OverflowError):
            _ = HUGE + HUGE
        with pytest.raises(OverflowError):
            _ = -HUGE - HUGE

    def test_area_and_volume(self):
        with pytest.raises(OverflowError):
            _ = HUGE * HUGE
        with pytest.raises(OverflowError):
            _ = (HUGE * cm(1)) * HUGE

    def test_division(self):
        with pytest.raises(OverflowError):
            _ = HUGE / 1e-10

    def test_offset_and_size(self):
        far = shapes.offset(x=HUGE)
        with pytest.raises(OverflowError):
            _ = far + far
        box = shapes.size(HUGE, cm(1), cm(1))
        with pytest.raises(OverflowError):
            _ = box + far
        with pytest.raises(OverflowError):
            _ = far + box


class TestPentestCases:
    """The cases of the pen-test report, docs/pentest-dollhouse-shapes.md, item 3."""

    def test_nan_vertex(self):
        solid = shapes.solid()
        solid.add_vert(cm(1), cm(0), cm(0))
        with pytest.raises(ValueError):
            solid.add_vert(cm(NAN), cm(0), cm(0))
        assert len(solid.vertices) == 1

    def test_infinite_cube(self):
        with pytest.raises(ZeroDivisionError):
            shapes.solid.cube(cm(1) / 0.0, cm(1), cm(1))

    def test_nan_radius(self):
        with pytest.raises(ValueError):
            shapes.solid.rounded_cube(cm(1), cm(1), cm(1), radius=cm(NAN))

    def test_nan_spinner(self):
        rng = shapes.RNG("0123456789abcdef0123456789abcdef")
        with pytest.raises(ValueError):
            rng.spinner(NAN, 1)
        with pytest.raises(ValueError):
            rng.spinner(0, INF)
