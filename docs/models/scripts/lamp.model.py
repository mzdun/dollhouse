# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

from shapes import *  # type: ignore

_wood = library.register_paint(rgb(226, 196, 152), roughness=0.55)
_black_plastic = library.register_paint(rgb(22, 22, 22), roughness=0.4)
_glass = library.register_paint(rgb(255, 242, 244, 0.35), roughness=0.05)


@library.model
def simple_lamp(root: group):
    sides = 32
    base = solid.revolve(
        [
            (cm(0), cm(0)),
            (cm(10), cm(0)),
            (cm(10), cm(2)),
            (cm(3), cm(2)),
            (cm(3), cm(30)),
            (cm(0), cm(30)),
        ],
        sides=sides,
    )

    bulb_sphere = solid.sphere(cm(4), longitudes=sides)
    bulb_neck = solid.prism(cm(4), cm(2.5), sides=sides).transform(
        I.translate(at_y(-cm(5)))
    )

    root.material = _wood
    base_group = root.add_group(name="base")
    base_group.add(base)

    socket = base_group.add_group(
        pos=at_y(cm(30)), name="socket", material=_black_plastic
    )
    socket.add(solid.prism(cm(2), cm(2.9), sides=sides))

    bulb = socket.add_group(pos=at_y(cm(6)), name="bulb")
    bulb.material = _glass
    bulb.add(cleanup(bulb_sphere | bulb_neck))
    bulb.add(light(name="bulb-light", color=rgb(255, 255, 255)))
