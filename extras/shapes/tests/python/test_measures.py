# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

import pytest
import shapes


class TestLength:
    def test_default_is_zero(self):
        assert shapes.length().value == 0
        assert shapes.length() == shapes.cm(0)

    @pytest.mark.parametrize(
        "make, amount, meters",
        [
            (shapes.cm, 2, 0.02),
            (shapes.cm, 150, 1.5),
            (shapes.mm, 5, 0.005),
            (shapes.mm, 1000, 1.0),
        ],
    )
    def test_units(self, make, amount, meters):
        assert make(amount).value == pytest.approx(meters)

    def test_conversions(self):
        assert shapes.cm(12).to_mm() == pytest.approx(120)
        assert shapes.mm(35).to_cm() == pytest.approx(3.5)

    # lengths are stored in meters, so e.g. cm(3) - cm(1) is 0.03 - 0.01,
    # which is not exactly 0.02; compare the results with a tolerance
    @pytest.mark.parametrize(
        "result, expected",
        [
            (shapes.cm(1) + shapes.mm(5), shapes.mm(15)),
            (shapes.cm(3) - shapes.cm(1), shapes.cm(2)),
            (shapes.cm(3) * 2, shapes.cm(6)),
            (2 * shapes.cm(3), shapes.cm(6)),
            (shapes.cm(3) / 2, shapes.mm(15)),
            (-shapes.cm(3), shapes.cm(-3)),
            (abs(shapes.cm(-3)), shapes.cm(3)),
        ],
        ids=["add", "sub", "mul", "rmul", "div", "neg", "abs"],
    )
    def test_arithmetic(self, result, expected):
        assert isinstance(result, shapes.length)
        assert result.value == pytest.approx(expected.value)

    def test_ordering(self):
        assert shapes.mm(9) < shapes.cm(1)
        assert shapes.cm(1) <= shapes.mm(10)
        assert sorted([shapes.cm(2), shapes.mm(3), shapes.cm(1)]) == [
            shapes.mm(3),
            shapes.cm(1),
            shapes.cm(2),
        ]

    def test_immutable_in_place_ops(self):
        a = shapes.cm(1)
        alias = a
        a += shapes.cm(1)
        assert a == shapes.cm(2)
        assert alias == shapes.cm(1)

    def test_hashable(self):
        assert len({shapes.cm(1), shapes.mm(10), shapes.cm(2)}) == 2

    def test_text(self):
        assert repr(shapes.cm(2)) == "cm(2)"
        assert str(shapes.cm(2)) == "2 cm"

    def test_text_past_micrometers(self):
        # more micrometers, than a 64-bit integer holds
        assert str(shapes.cm(1e20)) == "1e+20 cm"
        assert str(shapes.cm(-1e20)) == "-1e+20 cm"


class TestArea:
    def test_keywords(self):
        assert shapes.area(cm2=3) == shapes.area(mm2=300)
        assert shapes.area().value == 0

    def test_both_units_rejected(self):
        with pytest.raises(ValueError, match="not both"):
            shapes.area(cm2=1, mm2=1)

    def test_keyword_only(self):
        with pytest.raises(TypeError):
            shapes.area(3)  # type: ignore[call-arg]

    def test_repr(self):
        assert repr(shapes.area(cm2=3)) == "area(cm2=3)"
