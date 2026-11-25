# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Writes the glTF 2 models the verify tests read. Each model is a document
of nodes, lights and the PYXEL_interactions extra only, with no meshes and
no buffers, so the files stay small and readable, and each one holds
exactly what its tests look at, broken declarations included.

Run it from any directory; the files go next to this script."""

import json
import math
from pathlib import Path
from typing import Any

HERE = Path(__file__).parent

Json = dict[str, Any]


def light_node(name: str, light: int, area: bool = False) -> Json:
    extension = "PYXEL_lights_area" if area else "KHR_lights_punctual"
    return {"name": name, "extensions": {extension: {"light": light}}}


def document(
    nodes: list[Json],
    objects: list[Any] | None = None,
    punctual: list[Json] | None = None,
    area: list[Json] | None = None,
) -> Json:
    """The first node is the root of the scene; the active objects go to
    its extras."""
    if objects is not None:
        nodes[0].setdefault("extras", {})["PYXEL_interactions"] = objects

    result: Json = {
        "asset": {"version": "2.0", "generator": "verify tests"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
    }

    extensions: Json = {}
    if punctual:
        extensions["KHR_lights_punctual"] = {"lights": punctual}
    if area:
        extensions["PYXEL_lights_area"] = {"lights": area}
    if extensions:
        result["extensionsUsed"] = sorted(extensions)
        result["extensions"] = extensions

    return result


def door_object(**changes: Any) -> Json:
    """A door with two states: the base of most of the models below."""
    result: Json = {
        "name": "door",
        "version": "1.0",
        "states": {
            "closed": {},
            "opened": {"hinge": [{"rotate.y": math.radians(80)}]},
        },
        "actions": [
            {"label": "Open", "when": "closed", "then": "opened"},
            {"label": "Close", "when": "opened", "then": "closed"},
        ],
        "transitions": {"hinge": "rotate.y 200ms ease-in"},
    }
    result.update(changes)
    return result


def door_nodes() -> list[Json]:
    return [
        {"name": "door", "children": [1, 2]},
        {"name": "frame"},
        {"name": "hinge", "children": [3, 4]},
        {"name": "leaf"},
        {"name": "handle"},
    ]


# ---- models ------------------------------------------------------------


def hierarchy() -> Json:
    """Nested nodes, no lights and no active objects."""
    return document(
        [
            {"name": "house", "children": [1, 4]},
            {"name": "ground-floor", "children": [2, 3]},
            {"name": "kitchen"},
            {"name": "hall"},
            {"name": "attic", "children": [5]},
            {"name": "chest"},
        ]
    )


def lights() -> Json:
    """One light of each type; each named light becomes a light source."""
    return document(
        [
            {"name": "room", "children": [1, 2, 3, 4, 5]},
            light_node("sun", 0),
            light_node("bulb", 1),
            light_node("reading-lamp", 2),
            light_node("panel", 0, area=True),
            light_node("oval", 1, area=True),
        ],
        punctual=[
            {"type": "directional", "intensity": 2},
            {"type": "point", "range": 5},
            {"type": "spot", "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.6}},
        ],
        area=[
            {
                "shape": "rectangle",
                "width": 0.6,
                "height": 0.3,
                "spread": math.pi,
                "intensity": 10,
            },
            {
                "shape": "ellipse",
                "width": 0.4,
                "height": 0.2,
                "spread": math.pi / 2,
                "intensity": 10,
            },
        ],
    )


def light_source() -> Json:
    """A light source declared in the file, bringing its transition."""
    return document(
        [{"name": "lamp", "children": [1]}, light_node("bulb", 0)],
        objects=[
            {
                "name": "bulb",
                "version": "1.0",
                "transitions": {"light": "1.5s ease-out 100ms"},
            }
        ],
        punctual=[{"type": "point"}],
    )


def door() -> Json:
    """The geometry object showing every part of the printout."""
    return document(door_nodes(), objects=[door_object()])


def cabinet() -> Json:
    """Every kind of channel, slot, direction and easing."""
    slots = [
        {"name": "ratio", "type": "float", "direction": "input", "default": 0.25},
        {"name": "dim", "type": "factor", "direction": "input"},
        {"name": "opened", "type": "boolean", "direction": "output"},
        {
            "name": "tilt",
            "type": "angle",
            "direction": "output",
            "default": math.radians(15),
        },
        {"name": "reach", "type": "length", "direction": "variable"},
        {
            "name": "spot",
            "type": "offset",
            "direction": "variable",
            "default": [0.1, 0.2, 0.3],
        },
    ]
    states = {
        "closed": {},
        "ajar": {
            "drawer": [{"translate.z": 0.1}],
            "left-door": [{"rotate.y": math.radians(-30)}],
            "$opened": [{"set": True}],
            "$tilt": [{"set": math.radians(5)}],
        },
        "opened": {
            "drawer": [{"translate": [0.01, 0.02, 0.4]}],
            "left-door": [{"rotate": [0, math.radians(-90), 0]}],
            "knob": [{"scale.x": 1.5}, {"scale": [2, 2, 2]}],
            "$opened": [{"set": True}],
            "$reach": [{"set": 0.75}],
            "$spot": [{"set": [0.5, 0, -0.125]}],
        },
    }
    actions = [
        {"label": "Peek", "when": "closed", "then": "ajar"},
        {"label": "Open", "when": ["closed", "ajar"], "then": "opened"},
        {"label": "Close", "when": ["ajar", "opened"], "then": "closed"},
    ]
    transitions = {
        "*": "all 2s linear 50ms",
        "drawer": "translate.z 300ms ease, translate 400ms ease-in-out",
        "left-door": "all 250ms ease-out 10ms",
        "knob": "scale.x 0s ease-in, scale 1s",
        "$opened": "set 0s linear 45ms",
    }
    return document(
        [
            {"name": "cabinet", "children": [1, 2]},
            {"name": "drawer"},
            {"name": "left-door", "children": [3]},
            {"name": "knob"},
        ],
        objects=[
            {
                "name": "cabinet",
                "version": "1.0",
                "slots": slots,
                "states": states,
                "actions": actions,
                "transitions": transitions,
            }
        ],
    )


def unreachable() -> Json:
    """A state no action leads to, and the action leaving it."""
    states = {
        "closed": {},
        "opened": {"hinge": [{"rotate.y": math.radians(80)}]},
        "jammed": {"hinge": [{"rotate.y": math.radians(10)}]},
    }
    actions = [
        {"label": "Open", "when": "closed", "then": "opened"},
        {"label": "Close", "when": "opened", "then": "closed"},
        {"label": "Unjam", "when": "jammed", "then": "closed"},
    ]
    return document(door_nodes(), objects=[door_object(states=states, actions=actions)])


def dropped() -> Json:
    """Objects the importer leaves out, each for another reason; the
    door, which is fine, stays."""
    objects: list[Any] = [
        door_object(),
        "not a mapping",
        {"version": "1.0"},
        {"name": "no-version"},
        {"name": "not-a-version", "version": "one"},
        {"name": "too-new", "version": "2.0"},
        {"name": "too-old", "version": "0.9"},
        door_object(
            name="ghost",
            states={"closed": {}, "opened": {"ghost-hinge": [{"rotate.y": 1}]}},
            transitions={},
        ),
    ]
    return document(door_nodes(), objects=objects)


def newer() -> Json:
    """An object of a newer minor version, with a key this one does not
    know."""
    return document(
        door_nodes(),
        objects=[door_object(version="1.1", sounds={"opened": "creak.ogg"})],
    )


MODELS = {
    "hierarchy": hierarchy,
    "lights": lights,
    "light-source": light_source,
    "door": door,
    "cabinet": cabinet,
    "unreachable": unreachable,
    "dropped": dropped,
    "newer": newer,
}


def main() -> None:
    for name, build in MODELS.items():
        path = HERE / f"{name}.gltf"
        text = json.dumps(build(), indent=2) + "\n"
        path.write_text(text, encoding="UTF-8", newline="\n")
        print(path.relative_to(HERE).as_posix())

    # not JSON at all, for the importer to refuse
    (HERE / "broken.gltf").write_text("{ this is not glTF", encoding="UTF-8")
    print("broken.gltf")


if __name__ == "__main__":
    main()
