# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

from dataclasses import dataclass

from shapes import (
    I,
    action,
    at_x,
    at_y,
    at_z,
    channel,
    cleanup,
    cm,
    deg,
    group,
    length,
    library,
    mm,
    rgb,
    size,
    solid,
    state,
    transitions,
)

metal = library.register_paint(rgb(0, 0, 0), roughness=0.6, metallic=0.75)
plank = library.register_paint(rgb(218, 185, 141), roughness=0.6)

plank_width = cm(1)

shield_size = mm(55)
shield_width = mm(5)
grip_length = mm(128)
grip_height = mm(23)
handle_height = mm(24)
grip_depth = mm(10)

handle_x_right = mm(33)
handle_y = mm(1082)

handle_offset = (shield_size - grip_height) / 2

grip_x_right = handle_x_right + handle_offset


@dataclass
class door_cfg:
    wall_hole: size
    door_width: length
    casing_width: length


def _pi(hole: size, width: length):
    outline = hole.inflate(dx=2 * width, dy=width)
    block = solid.cube(outline.width, outline.height, outline.depth).transform(
        I.translate(x=-outline.width / 2, z=-outline.depth / 2)
    )
    stamp = solid.cube(hole.width, hole.height + cm(100), hole.depth * 2).transform(
        I.translate(x=-hole.width / 2, y=-cm(100), z=-hole.depth)
    )
    return block - stamp


def _door_frame(hole: size, casing_width: length, door_width: length):
    casing_size = hole.inflate(dx=2 * -plank_width, dy=-plank_width, dz=2 * plank_width)
    s = _pi(casing_size, casing_width)
    if casing_width > plank_width:
        s -= _pi(
            hole,
            casing_width,
        )
    inner_door_width = door_width - plank_width
    inside_wall = inner_door_width - plank_width
    s |= _pi(
        hole.inflate(dx=4 * -plank_width, dy=2 * -plank_width, dz=-2 * inside_wall),
        plank_width,
    )
    return cleanup(s)


def _door_pane(hole: size, door_width: length):
    rebated = solid.cube(hole.width, hole.height, plank_width).transform(
        I.translate(x=-hole.width / 2)
    )
    inner_width = door_width - plank_width
    inner = solid.cube(
        hole.width - 2 * plank_width,
        hole.height - plank_width,
        inner_width,
    ).transform(I.translate(x=-hole.width / 2 + plank_width, z=-inner_width))
    return cleanup(rebated | inner)


def _box(x: length, y: length, z: length, w: length, h: length, d: length):
    """Cube of size (w, h, d) with its minimal corner at (x, y, z)."""
    return solid.cube(w, h, d).transform(I.translate(x=x, y=y, z=z))


def _door_handle(door_width: length):
    """Handle going through a door of `door_width`; the origin is in the center
    of the upper shield (also the middle of the door's depth), the grips point
    towards -x and the lower (key) shield is below, along -y."""
    shield_depth = door_width + 2 * shield_width
    handle_depth = door_width + 2 * handle_height

    def shield(y: length):
        return _box(
            -shield_size / 2,
            y - shield_size / 2,
            -shield_depth / 2,
            shield_size,
            shield_size,
            shield_depth,
        )

    def grip(z: length):
        return _box(
            grip_height / 2 - grip_length,
            -grip_height / 2,
            z,
            grip_length,
            grip_height,
            grip_depth,
        )

    neck = _box(
        -grip_height / 2,
        -grip_height / 2,
        -handle_depth / 2,
        grip_height,
        grip_height,
        handle_depth,
    )

    door_handle = (
        shield(cm(0))
        | shield(-3 * shield_size)
        | neck
        | grip(-handle_depth / 2 - grip_depth)
        | grip(handle_depth / 2)
    )
    return cleanup(door_handle)


@library.model
def door(root: group):
    cfg = door_cfg(size(mm(844), mm(2035), cm(8)), mm(38), mm(62))
    root.material = plank

    casing = root.add_group(name="casing")
    casing.add(
        _door_frame(
            hole=cfg.wall_hole,
            casing_width=cfg.casing_width or plank_width,
            door_width=cfg.door_width,
        )
    )
    hinge_pos = cfg.wall_hole.width / 2 + plank_width

    hinge_rot = root.add_group(name="hinge").transform(
        I.translate(x=-hinge_pos, z=cfg.wall_hole.depth / 2 + plank_width)
    )
    hinge_rot.add_active_object(
        name="opener",
        states=[
            state("closed"),
            state("opened").transform(group="hinge", change=channel.rotate_y(-deg(92))),
        ],
        actions=[
            action(label="Open", when="closed", then="opened"),
            action(label="Close", when="opened", then="closed"),
        ],
        transitions=transitions().for_all("150ms"),
    )

    hinge_rot.add_group(name="pane").add(
        _door_pane(cfg.wall_hole, cfg.door_width).transform(I.translate(x=hinge_pos))
    )

    handle_shape = _door_handle(door_width=cfg.door_width)

    handle_x = handle_x_right + shield_size / 2
    handle_x = cfg.wall_hole.width + plank_width - handle_x
    handle_z = plank_width - cfg.door_width / 2

    handle = hinge_rot.add_group(
        pos=at_x(handle_x) + at_y(handle_y) + at_z(handle_z),
        name="handle",
        material=metal,
    )

    handle.add(handle_shape)
