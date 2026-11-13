# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

import re

import pytest
import shapes

SEED = "0123456789abcdef0123456789abcdef"


def test_seedless_prints_its_seed(capsys: pytest.CaptureFixture[str]):
    rng = shapes.RNG()
    assert re.fullmatch(r"[0-9a-fA-F]{32}", rng.seed)
    assert rng.seed in capsys.readouterr().out


def test_valid_seed_is_kept_quietly(capsys: pytest.CaptureFixture[str]):
    rng = shapes.RNG(SEED)
    assert rng.seed == SEED
    assert capsys.readouterr().out == ""


def test_invalid_seed_is_replaced(capsys: pytest.CaptureFixture[str]):
    rng = shapes.RNG("not a seed")
    assert rng.seed != "not a seed"
    assert rng.seed in capsys.readouterr().out


def test_same_seed_same_rolls():
    first = shapes.RNG(SEED).dice(6)
    second = shapes.RNG(SEED).dice(6)
    assert [first.roll() for _ in range(32)] == [second.roll() for _ in range(32)]


@pytest.mark.parametrize("sides", [1, 2, 6, 20])
def test_dice_range(sides: int):
    dice = shapes.RNG(SEED).dice(sides)
    assert (dice.min, dice.max) == (1, sides)
    assert all(1 <= dice.roll() <= sides for _ in range(200))


def test_dice_needs_a_side():
    with pytest.raises(ValueError, match="at least one side"):
        shapes.RNG(SEED).dice(0)


def test_dice_ends_are_swapped():
    dice = shapes.RNG(SEED).dice(10, 5)
    assert (dice.min, dice.max) == (5, 10)


def test_spinner_range():
    spinner = shapes.RNG(SEED).spinner(-1.0, 1.0)
    assert all(-1.0 <= spinner.spin() < 1.0 for _ in range(200))


def test_dice_keeps_rng_alive():
    dice = shapes.RNG(SEED).dice(6)  # the RNG has no other reference
    assert 1 <= dice.roll() <= 6
