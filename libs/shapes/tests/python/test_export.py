# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Normals of the exported meshes: flat, one per face, agreeing with the
winding of the face and pointing out of the solid."""

import math
from pathlib import Path

import pytest
import shapes
from shapes import I, cm, solid

Vec = tuple[float, float, float]


def sub(a: Vec, b: Vec) -> Vec:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a: Vec, b: Vec) -> Vec:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def dot(a: Vec, b: Vec) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def unit(a: Vec) -> Vec:
    length = math.sqrt(dot(a, a))
    return (a[0] / length, a[1] / length, a[2] / length)


def export_obj(tmp_path: Path, fill) -> Path:
    def normals_model(root: shapes.group):
        fill(root)
        return "obj"

    shapes.library.model(normals_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    return path


def read_obj(path: Path) -> list[list[tuple[Vec, Vec]]]:
    """Triangles of an OBJ file, as (position, normal) per corner."""
    positions: list[Vec] = []
    normals: list[Vec] = []
    faces: list[list[tuple[Vec, Vec]]] = []
    for line in path.read_text(encoding="UTF-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            positions.append(tuple(float(x) for x in parts[1:4]))  # type: ignore
        elif parts[0] == "vn":
            normals.append(tuple(float(x) for x in parts[1:4]))  # type: ignore
        elif parts[0] == "f":
            face = []
            for corner in parts[1:]:
                v, _, n = corner.split("/")
                face.append((positions[int(v) - 1], normals[int(n) - 1]))
            faces.append(face)
    return faces


@pytest.mark.parametrize(
    "matrix",
    [I, I.scale(-1, 1, 1)],
    ids=["plain", "mirrored"],
)
def test_box_normals_are_flat_and_outwards(tmp_path: Path, matrix):
    path = export_obj(
        tmp_path,
        lambda root: root.add(solid.cube(cm(2), cm(2), cm(2)).transform(matrix)),
    )
    faces = read_obj(path)
    assert len(faces) == 12

    corners = [p for face in faces for p, _ in face]
    centre: Vec = tuple(sum(p[i] for p in corners) / len(corners) for i in range(3))  # type: ignore

    for face in faces:
        (a, _), (b, _), (c, _) = face[:3]
        winding = unit(cross(sub(b, a), sub(c, a)))
        face_centre: Vec = tuple((a[i] + b[i] + c[i]) / 3 for i in range(3))  # type: ignore
        assert dot(winding, sub(face_centre, centre)) > 0, "face wound inwards"
        for _, normal in face:
            assert dot(winding, unit(normal)) == pytest.approx(1.0, abs=1e-5)
