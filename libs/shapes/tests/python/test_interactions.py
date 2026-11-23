# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""Active objects declared from Python: the helper structures, the members
of `group`, the export to the PYXEL_interactions extra, and the builders
shared by threads (see The shapes module in docs/model-interactions.md)."""

import json
import math
import threading
from pathlib import Path
from typing import Any, Callable

import pytest
import shapes
from shapes import *  # type: ignore

Fill = Callable[[shapes.group], None]


def build(
    tmp_path: Path, fill: Fill, exporter: str = "gltf2"
) -> tuple[list[Any], Path]:
    """The active objects of the root group after `fill`, and the exported
    file."""
    objects: list[Any] = []

    def interactions_model(root: shapes.group) -> str:
        fill(root)
        objects.extend(root.active_objects)
        return exporter

    shapes.library.model(interactions_model)
    shapes.library.output_directory = tmp_path
    try:
        (path,) = shapes.library.build_models()
    finally:
        shapes.library.clear_models()
    return objects, path


def exported(path: Path) -> list[dict[str, Any]]:
    """The PYXEL_interactions extra of an exported glTF file."""
    document = json.loads(path.read_text(encoding="UTF-8"))
    extras = [
        node["extras"]["PYXEL_interactions"]
        for node in document["nodes"]
        if "PYXEL_interactions" in node.get("extras", {})
    ]
    assert len(extras) == 1
    return extras[0]


def door(parent: shapes.group) -> None:
    hinge = parent.add_group(name="door-hinge")
    hinge.add(solid.cube(cm(80), cm(200), cm(4)))
    parent.add_active_object(
        name="door",
        slots=[
            slot(name="opened", type=slot_type.boolean, direction=slot_direction.output)
        ],
        states=[
            state("closed"),
            state("opened")
            .transform(hinge.name, channel.rotate_y(deg(80)))
            .set("opened", True),
        ],
        actions=[
            action(label="Open", when="closed", then="opened"),
            action(label="Close", when="opened", then="closed"),
        ],
        transitions=transitions().for_group(hinge.name, "rotate.y 200ms ease-in"),
    )


class TestChannel:
    def test_single_axis(self):
        change = channel.rotate_y(deg(80))
        assert change.around == axis.y
        assert change.by == deg(80)
        assert channel.translate_z(cm(4)).on == axis.z
        assert channel.scale_x(2).along == axis.x

    def test_vector_channels_fill_the_defaults(self):
        assert channel.translate(cm(1)).by == offset(cm(1), cm(0), cm(0))
        assert channel.translate(offset(cm(1), cm(2), cm(3))) == channel.translate(
            cm(1), cm(2), cm(3)
        )
        turned = channel.rotate(deg(10))
        assert (turned.x, turned.y, turned.z) == (deg(10), deg(0), deg(0))
        scaled = channel.scale(2)
        assert (scaled.x, scaled.y, scaled.z) == (2, 1, 1)

    def test_repr_evaluates_back(self):
        for change in [
            channel.translate_x(cm(40)),
            channel.translate(cm(1), cm(2), cm(3)),
            channel.rotate_y(-deg(80)),
            channel.rotate(deg(1), deg(2), deg(3)),
            channel.scale_z(0.5),
            channel.scale(1, 2, 3),
        ]:
            assert eval(repr(change)) == change  # pylint: disable=eval-used
        assert repr(channel.rotate_y(deg(80))) == "channel.rotate_y(deg(80))"

    def test_attributes_are_copies(self):
        change = channel.translate(offset(cm(1), cm(2), cm(3)))
        moved = change.by
        moved[axis.x] = cm(9)
        assert change.by == offset(cm(1), cm(2), cm(3))

    def test_scale_must_be_finite(self):
        with pytest.raises(ValueError):
            channel.scale_x(math.nan)
        with pytest.raises(ValueError):
            channel.scale(1, math.inf)


class TestSlot:
    def test_arguments(self):
        enabled = slot(
            name="enabled", type=slot_type.boolean, direction=slot_direction.input
        )
        assert enabled.name == "enabled"
        assert enabled.type == slot_type.boolean
        assert enabled.direction == slot_direction.input
        assert enabled.default_value is None
        assert slot("level", slot_type.float, slot_direction.variable, 0.5) == slot(
            name="level",
            type=slot_type.float,
            direction=slot_direction.variable,
            default=0.5,
        )

    def test_default_keeps_bools_apart_from_numbers(self):
        def default(value: object) -> object:
            return slot(
                "x", slot_type.float, slot_direction.output, value
            ).default_value

        assert default(True) is True
        assert isinstance(default(1), float)
        assert default(1) == 1.0
        assert default(cm(3)) == cm(3)
        assert default(deg(30)) == deg(30)
        assert default(offset(cm(1))) == offset(cm(1))

    def test_default_of_a_wrong_type(self):
        with pytest.raises(TypeError, match="slot: default"):
            slot("x", slot_type.float, slot_direction.output, "1")
        with pytest.raises(ValueError):
            slot("x", slot_type.float, slot_direction.output, math.nan)
        with pytest.raises(OverflowError):
            slot("x", slot_type.float, slot_direction.output, 10**400)

    def test_repr_evaluates_back(self):
        value = slot("level", slot_type.length, slot_direction.output, cm(5))
        assert eval(repr(value)) == value  # pylint: disable=eval-used


class TestState:
    def test_calls_chain_on_the_same_state(self):
        opened = state("opened")
        assert opened.transform("hinge", channel.rotate_y(deg(80))) is opened
        assert opened.transform("leaf", []) is opened
        assert opened.set("enabled", True) is opened

    def test_changes_keep_their_order(self):
        opened = (
            state("opened")
            .transform("leaf", channel.translate_x(cm(1)))
            .transform("leaf", [channel.rotate_y(deg(5)), channel.scale(2)])
        )
        assert opened.name == "opened"
        assert opened.groups == {
            "leaf": [
                channel.translate_x(cm(1)),
                channel.rotate_y(deg(5)),
                channel.scale(2),
            ]
        }

    def test_set_keeps_bools_apart_from_numbers(self):
        changed = state("on").set("enabled", True).set("level", 1).set("size", cm(2))
        slots = changed.slots
        assert slots["enabled"][0].set is True
        assert isinstance(slots["level"][0].set, float)
        assert slots["size"][0].set == cm(2)
        with pytest.raises(TypeError, match="state.set"):
            state("on").set("level", "1")
        with pytest.raises(TypeError):
            state("on").transform("leaf", cm(1))

    def test_attributes_are_copies(self):
        opened = state("opened").transform("hinge", channel.rotate_y(deg(80)))
        opened.groups["hinge"].append(channel.scale(2))
        assert opened.groups == {"hinge": [channel.rotate_y(deg(80))]}

    def test_equality(self):
        assert state("a").set("x", 1) == state("a").set("x", 1.0)
        assert state("a") != state("b")
        assert state("a") != "a"

    def test_repr_evaluates_back(self):
        opened = (
            state("opened")
            .transform("right", channel.rotate_y(deg(80)))
            .transform("left", [channel.rotate_y(-deg(80)), channel.translate_x(cm(1))])
            .set("enabled", True)
        )
        assert eval(repr(opened)) == opened  # pylint: disable=eval-used
        assert repr(state("closed")) == "state('closed')"


class TestAction:
    def test_when_takes_a_name_or_a_list(self):
        assert action("Open", "closed", "opened").when == ["closed"]
        close = action(label="Close", when=["tilted", "opened"], then="closed")
        assert (close.label, close.when, close.then) == (
            "Close",
            ["tilted", "opened"],
            "closed",
        )
        with pytest.raises(TypeError):
            action("Open", 1, "opened")

    def test_repr_evaluates_back(self):
        for value in [
            action("Open", "closed", "opened"),
            action("Close", ["tilted", "opened"], "closed"),
        ]:
            assert eval(repr(value)) == value  # pylint: disable=eval-used


class TestTransitions:
    def test_calls_chain_and_replace(self):
        rules = transitions()
        assert rules.defaults is None
        assert rules.for_all("200ms") is rules
        assert rules.for_group("hinge", "rotate.y 1s") is rules
        assert rules.for_group("hinge", "rotate.y 2s") is rules
        assert rules.for_slot("enabled", "0ms linear 45ms") is rules
        assert rules.defaults == "200ms"
        assert rules.groups == {"hinge": "rotate.y 2s"}
        assert rules.slots == {"enabled": "0ms linear 45ms"}

    def test_repr_evaluates_back(self):
        rules = (
            transitions()
            .for_all("rotate.x 200ms ease-in")
            .for_group("tilt-hinge", "rotate.x 300ms ease-in")
            .for_slot("enabled", "0ms")
        )
        assert eval(repr(rules)) == rules  # pylint: disable=eval-used
        assert repr(transitions()) == "transitions()"


class TestGroup:
    def test_declarations_are_copies(self, tmp_path: Path):
        opened = state("opened").transform("hinge", channel.rotate_y(deg(80)))
        rules = transitions().for_group("hinge", "1s")

        def fill(root: shapes.group):
            hinge = root.add_group(name="hinge")
            hinge.add(solid.cube(cm(1), cm(1), cm(1)))
            hinge.add(light(name="lamp"))
            assert (
                root.add_active_object(name="door", states=[opened], transitions=rules)
                is root
            )
            opened.set("late", True)
            rules.for_all("2s")
            assert root.set_light_transition("lamp", "150ms") is root
            assert root.set_light_transition("lamp", "300ms") is root

        objects, _ = build(tmp_path, fill)
        assert len(objects) == 2
        door_object, lamp = objects[0], objects[1]
        assert isinstance(door_object, geo_active_object)
        assert door_object.name == "door"
        assert door_object.slots == []
        assert door_object.actions == []
        assert door_object.states == [
            state("opened").transform("hinge", channel.rotate_y(deg(80)))
        ]
        assert door_object.transitions == transitions().for_group("hinge", "1s")
        assert isinstance(lamp, light_active_object)
        assert (lamp.name, lamp.light) == ("lamp", "300ms")

    def test_arguments_are_keyword_only(self, tmp_path: Path):
        def fill(root: shapes.group):
            with pytest.raises(TypeError):
                root.add_active_object("door")  # type: ignore[misc]
            with pytest.raises(TypeError):
                root.add_active_object(name="door", states=["closed"])  # type: ignore[list-item]
            with pytest.raises(TypeError):
                root.add_active_object(name="door", transitions="1s")  # type: ignore[arg-type]
            root.add_group(name="leaf").add(solid.cube(cm(1), cm(1), cm(1)))

        objects, _ = build(tmp_path, fill)
        assert not objects

    def test_export(self, tmp_path: Path):
        def fill(root: shapes.group):
            door(root)
            root.add_group(name="lamp").add(light(name="lamp"))
            root.set_light_transition("lamp", "150ms")

        _, path = build(tmp_path, fill)
        door_object, lamp = exported(path)
        assert door_object["name"] == "door"
        assert door_object["slots"] == [
            {"name": "opened", "type": "boolean", "direction": "output"}
        ]
        assert door_object["states"]["closed"] == {}
        opened = door_object["states"]["opened"]
        assert opened["$opened"] == [{"set": True}]
        assert opened["door-hinge"][0]["rotate.y"] == pytest.approx(math.radians(80))
        assert door_object["actions"] == [
            {"label": "Open", "when": "closed", "then": "opened"},
            {"label": "Close", "when": "opened", "then": "closed"},
        ]
        assert door_object["transitions"] == {"door-hinge": "rotate.y 200ms ease-in"}
        assert lamp == {
            "name": "lamp",
            "version": "1.0",
            "transitions": {"light": "150ms"},
        }


class TestThreads:
    """The builders shared by threads; on a free-threaded interpreter, the
    critical sections keep every change, with the GIL, the GIL does."""

    THREADS = 8
    ROUNDS = 50

    def run(self, work: Callable[[int], None]) -> None:
        errors: list[BaseException] = []
        start = threading.Barrier(self.THREADS)

        def worker(index: int) -> None:
            try:
                start.wait()
                work(index)
            except BaseException as error:  # pylint: disable=broad-exception-caught
                errors.append(error)

        threads = [
            threading.Thread(target=worker, args=(index,))
            for index in range(self.THREADS)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        assert not errors

    def test_shared_state(self):
        shared = state("shared")

        def work(index: int) -> None:
            for round_index in range(self.ROUNDS):
                shared.transform("leaf", channel.translate_x(cm(round_index)))
                shared.set(f"slot{index}", round_index)
                assert len(repr(shared)) > 0
                assert shared == shared  # pylint: disable=comparison-with-itself

        self.run(work)
        assert len(shared.groups["leaf"]) == self.THREADS * self.ROUNDS
        slots = shared.slots
        assert len(slots) == self.THREADS
        assert all(len(changes) == self.ROUNDS for changes in slots.values())

    def test_shared_transitions(self):
        shared = transitions()

        def work(index: int) -> None:
            for round_index in range(self.ROUNDS):
                shared.for_group(f"group{index}-{round_index}", "1s")
                shared.for_all(f"{round_index}ms")
                assert len(repr(shared)) > 0

        self.run(work)
        assert len(shared.groups) == self.THREADS * self.ROUNDS

    def test_shared_group(self, tmp_path: Path):
        shared_state = state("opened")
        objects: list[Any] = []

        def fill(root: shapes.group):
            root.add_group(name="leaf").add(solid.cube(cm(1), cm(1), cm(1)))

            def work(index: int) -> None:
                for round_index in range(self.ROUNDS):
                    shared_state.transform("leaf", channel.rotate_y(deg(1)))
                    root.add_active_object(
                        name=f"object{index}-{round_index}", states=[shared_state]
                    )
                    root.set_light_transition(f"lamp{index}", f"{round_index}ms")
                    assert len(root.active_objects) > 0

            self.run(work)
            objects.extend(root.active_objects)

        # every copy of the shared state sets `rotate.y` of the leaf more
        # than once, and the lamps are missing: the export refuses them all
        with pytest.raises(RuntimeError, match="twice"):
            build(tmp_path, fill)
        assert len(objects) == self.THREADS * self.ROUNDS + self.THREADS
        assert len(shared_state.groups["leaf"]) == self.THREADS * self.ROUNDS
        for declaration in objects:
            if isinstance(declaration, geo_active_object):
                (copy,) = declaration.states
                assert 0 < len(copy.groups["leaf"]) <= self.THREADS * self.ROUNDS
