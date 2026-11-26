# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

from shapes import (
    I,
    action,
    at_x,
    at_z,
    channel,
    deg,
    group,
    length,
    library,
    mm,
    offset,
    rgb,
    slot,
    slot_direction,
    slot_type,
    solid,
    state,
    transitions,
)

_plastic = library.register_paint(rgb(246, 246, 242), roughness=0.35)

# the frame: a wide frustum, standing on the wall (z = 0), facing +z
_plate = mm(80)  # back of the frame, on the wall
_face = mm(74)  # front of the frame
_frame_depth = mm(9)

# the opening for the rockers, with rounded corners
_opening = mm(50)
_opening_radius = mm(2.5)
_floor = mm(2)  # what is left of the frame behind the opening

# the rockers
_gap = mm(0.75)  # between the rockers, and around them
_rocker_back = mm(5)
_rocker_front = mm(12)
_rocker_tilt = deg(4)


def _frustum(back: length, front: length, depth: length):
    """Square frustum, `back` wide at z = 0 and `front` wide at z = `depth`,
    centered on the z axis."""
    b, f = back / 2, front / 2
    vertices = [
        offset(-b, -b, mm(0)),
        offset(b, -b, mm(0)),
        offset(b, b, mm(0)),
        offset(-b, b, mm(0)),
        offset(-f, -f, depth),
        offset(f, -f, depth),
        offset(f, f, depth),
        offset(-f, f, depth),
    ]
    faces = [
        [3, 2, 1, 0],  # back, -z
        [4, 5, 6, 7],  # front, +z
        [0, 1, 5, 4],  # bottom, -y
        [1, 2, 6, 5],  # right, +x
        [2, 3, 7, 6],  # top, +y
        [3, 0, 4, 7],  # left, -x
    ]
    return solid(vertices, faces)


def _centered_box(width: length, height: length, z0: length, z1: length):
    """Box centered on the z axis, from z0 to z1."""
    return solid.cube(width, height, z1 - z0).transform(
        I.translate(x=-width / 2, y=-height / 2, z=z0)
    )


def _rounded_opening(edge: length, radius: length, z0: length, z1: length):
    """Square prism with rounded vertical corners, centered on the z axis,
    from z0 to z1. The rounded cube sticks out on both ends, so the rounding
    of its front and back edges is cut off by the slab."""
    margin = 2 * radius
    rounded = solid.rounded_cube(edge, edge, z1 - z0 + 2 * margin, radius).transform(
        I.translate(x=-edge / 2, y=-edge / 2, z=z0 - margin)
    )
    return rounded & _centered_box(edge, edge, z0, z1)


def _frame():
    stamp = _rounded_opening(_opening, _opening_radius, _floor, _frame_depth + mm(1))
    return _frustum(_plate, _face, _frame_depth) - stamp


def _rocker(side: int):
    """A rocker filling one half of the opening; rounded only at the corners
    following the opening, square along the middle gap. `side` is -1 for the
    left rocker, +1 for the right one."""
    edge = _opening - 2 * _gap
    insert = _rounded_opening(edge, _opening_radius - _gap, _rocker_back, _rocker_front)
    half_width = (edge - _gap) / 2
    half = _centered_box(half_width, edge, _rocker_back, _rocker_front).transform(
        I.translate(x=side * (_gap + half_width) / 2)
    )
    return insert & half


def _rocker_group(parent: group, name: str, side: int):
    """The rocker turns around the horizontal line through its middle, so the
    named group sits there and the geometry is moved back by the same."""
    pivot = at_x(side * (_gap / 2 + (_opening - 3 * _gap) / 4)) + at_z(
        (_rocker_back + _rocker_front) / 2
    )
    rocker = parent.add_group(name=name, pos=pivot)
    rocker.add(_rocker(side).transform(I.translate(-pivot)))

    # off: top away from the wall's viewer; on: top towards them
    parent.add_active_object(
        name=name,
        slots=[
            slot(
                name="enabled", type=slot_type.boolean, direction=slot_direction.output
            )
        ],
        states=[
            state("off")
            .transform(name, channel.rotate_x(-_rocker_tilt))
            .set("enabled", False),
            state("on")
            .transform(name, channel.rotate_x(_rocker_tilt))
            .set("enabled", True),
        ],
        actions=[
            action(label="Turn on", when="off", then="on"),
            action(label="Turn off", when="on", then="off"),
        ],
        transitions=transitions().for_all("80ms"),
    )


@library.model
def light_switch_double(root: group):
    """Two-rocker light switch; the origin is in the middle of the back plate,
    the switch faces +z."""
    root.name = "Two-rocker light switch"
    root.material = _plastic
    root.add_group(name="frame").add(_frame())
    _rocker_group(root, "left-rocker", -1)
    _rocker_group(root, "right-rocker", +1)
