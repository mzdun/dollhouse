# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""The `extent` overloads of the box-like solids match the edge-wise ones."""

import pytest
from shapes import cm, mm, size, solid

extent = size(cm(3), cm(2), cm(1))


def same(a: solid, b: solid) -> bool:
    return a.vertices == b.vertices and a.faces == b.faces


def test_cube_from_extent():
    assert same(solid.cube(extent), solid.cube(cm(3), cm(2), cm(1)))


def test_rounded_cube_from_extent():
    assert same(
        solid.rounded_cube(extent, mm(2)),
        solid.rounded_cube(cm(3), cm(2), cm(1), mm(2)),
    )
    assert same(
        solid.rounded_cube(extent=extent, radius=mm(2), segments=2),
        solid.rounded_cube(cm(3), cm(2), cm(1), mm(2), 2),
    )


def test_oval_rounded_cube_from_extent():
    assert same(
        solid.oval_rounded_cube(extent, mm(2)),
        solid.oval_rounded_cube(cm(3), cm(2), cm(1), mm(2)),
    )
    assert same(
        solid.oval_rounded_cube(extent, mm(2), rz=mm(1), segments=2),
        solid.oval_rounded_cube(cm(3), cm(2), cm(1), mm(2), rz=mm(1), segments=2),
    )


def test_extent_is_validated():
    with pytest.raises(ValueError):
        solid.cube(size(cm(0), cm(1), cm(1)))
