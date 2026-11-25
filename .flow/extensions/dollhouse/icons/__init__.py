# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

import os

from proj_flow.api import env, step
from proj_flow.api.makefile import Makefile

from . import magick

ICONS = os.path.join("data", "icons")
ASSETS = os.path.join("data", "assets")
ICONS_PNG = os.path.join(ICONS, "png")
SIZES = [16, 24, 32, 48, 256]
ALTS = [("tiny", "small", "medium"), ("small", "medium"), ("medium",)]


def prepare_rules(icon_name: str):
    stencil = os.path.join(ICONS_PNG, f"{icon_name}.png")
    stencils: dict[int, str] = {size: stencil for size in SIZES}
    additional: dict[str, str] = {}

    for index in range(len(ALTS)):
        alts = ALTS[index]
        for alt in alts:
            source = os.path.join(ICONS, f"{icon_name}-{alt}.svg")
            out_name = os.path.join(ICONS_PNG, f"{icon_name}-{alt}.png")
            if os.path.exists(source):
                additional[out_name] = source
                stencils[SIZES[index]] = out_name

    statements = [
        magick.svg_to_png(
            output=stencil,
            image=os.path.join(ICONS, f"{icon_name}.svg"),
        ),
    ]

    statements.extend(
        magick.svg_to_png(output=output, image=image)
        for output, image in additional.items()
    )

    statements.extend(
        magick.resize(
            output=os.path.join(ICONS_PNG, f"{icon_name}-{size}.png"),
            stencil=stencils[size],
            size=str(size),
        )
        for size in SIZES
    )

    statements.extend(
        [
            magick.merge(
                os.path.join(ASSETS, f"{icon_name}.ico"),
                [os.path.join(ICONS_PNG, f"{icon_name}-{size}.png") for size in SIZES],
            ),
            magick.mkdirs(ICONS_PNG),
            magick.copy(
                os.path.join(ICONS_PNG, f"{icon_name}-256.png"),
                os.path.join(ASSETS, f"{icon_name}.png"),
            ),
        ]
    )

    return statements


rules = [prepare_rules(icon_name) for icon_name in ["dollhouse", "inspector"]]
makefile = Makefile([rule for ruleset in rules for rule in ruleset])


@step.register
class IconsStep:
    name = "Icons"
    runs_before = ["Build"]

    def platform_dependencies(self):
        return [f"{magick.tool}>=6"]

    def run(self, config: env.Config, rt: env.Runtime):
        makefile.run(rt)
