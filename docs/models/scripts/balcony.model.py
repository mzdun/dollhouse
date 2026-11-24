# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

from shapes import *  # type: ignore

_frame_material = library.register_paint(rgb(255, 255, 255), roughness=0.9)
_glass_material = library.register_paint(rgb(61, 162, 205, 0.43), roughness=0.1)

_offset = mm(25)
_door_size = size(width=cm(180), height=cm(230), depth=cm(8))
_pane_depth = cm(4)
_left_width = cm(75)
_pane_frame_width = cm(10)


def _frame_solid(outline: size, frame_width: length):
    hole = outline.inflate(dx=-2 * frame_width, dy=-2 * frame_width, dz=4 * frame_width)
    block = solid.cube(outline)
    stamp = solid.cube(hole).transform(
        I.translate(x=frame_width, y=frame_width, z=-frame_width)
    )
    return block - stamp


def _glass_pane(outline: size, frame_width: length, glass_width: length):
    glass_size = outline.inflate(dx=-2 * frame_width, dy=-2 * frame_width)
    glass_size.depth = glass_width
    return solid.cube(glass_size).transform(
        I.translate(x=frame_width, y=frame_width, z=(outline.depth - glass_width) / 2)
    )


def _frame(parent: group, *, outline: size, frame_width: length, glass_width: length):
    result = parent.add_group()
    result.add(_frame_solid(outline, frame_width))
    result.add_group(material=_glass_material).add(
        _glass_pane(outline, frame_width, glass_width)
    )
    return result


def _tilt_and_turn(parent: group):
    # A named group sits on the pivot of its pane, so the states turn it
    # around that point; its child moves the pane back into place.
    left_pivot = at_z(_door_size.depth)
    left = parent.add_group(name="left-pane", pos=left_pivot).add_group(pos=-left_pivot)

    # Tilt-and-turn: the pivot is the bottom right corner on the room side,
    # so rotate_y turns the pane around its right edge and rotate_x tilts
    # it around its bottom edge.
    right_pivot = at_x(_door_size.width) + at_z(_door_size.depth)
    right = parent.add_group(name="right-pane", pos=right_pivot).add_group(
        pos=at_x(_left_width) - right_pivot
    )

    _frame(
        left,
        outline=_door_size.replace(width=_left_width),
        frame_width=_pane_frame_width,
        glass_width=_pane_depth,
    )

    _frame(
        right,
        outline=_door_size.inflate(dx=-_left_width),
        frame_width=_pane_frame_width,
        glass_width=_pane_depth,
    )


@library.model
def balcony_door(root: group):
    wide = deg(98)
    tilt = deg(5)

    root.material = _frame_material
    root.transform(
        I.translate(at_x(-_door_size.width / 2) + at_z(-_door_size.depth / 2))
    )
    frame = root.add_group(name="frame")
    frame.add(
        _frame_solid(
            outline=_door_size.inflate(dx=2 * _offset, dy=2 * _offset),
            frame_width=_offset,
        )
    )

    panes = frame.add_group(pos=at_x(_offset) + at_y(_offset))
    _tilt_and_turn(panes)

    root.add_active_object(
        name="tilt-and-turn",
        states=[
            state("closed"),
            state("tilted").transform("right-pane", change=channel.rotate_x(tilt)),
            state("opened").transform("right-pane", change=channel.rotate_y(wide)),
            state("opened-wide")
            .transform("right-pane", change=channel.rotate_y(wide))
            .transform("left-pane", change=channel.rotate_y(-wide)),
        ],
        actions=[
            action(label="Tilt", when="closed", then="tilted"),
            action(label="Open one", when="closed", then="opened"),
            # before "Close", so that the default interaction in "opened"
            # opens the second wing
            action(label="Open both", when="opened", then="opened-wide"),
            action(label="Close", when=["tilted", "opened"], then="closed"),
            action(label="Close left", when="opened-wide", then="opened"),
        ],
        transitions=transitions().for_all("200ms"),
    )
