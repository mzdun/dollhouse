# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""
Selects the `shapes` module under test.

When ``SHAPES_MODULE_DIR`` is set (CTest sets it to ``<build dir>/bin``), the
module is imported from that directory and nowhere else, so a test run never
silently falls back to an older copy installed in site-packages.
"""

import itertools
import os
import re
import subprocess
import sys
import textwrap
from collections.abc import Callable, Iterator
from pathlib import Path

import pytest

_MODULE_DIR = os.environ.get("SHAPES_MODULE_DIR")

if _MODULE_DIR:
    _module_dir = Path(_MODULE_DIR).resolve()
    sys.path.insert(0, str(_module_dir))

import shapes  # noqa: E402

if _MODULE_DIR:
    _loaded_from = Path(shapes.__file__).resolve()
    if not _loaded_from.is_relative_to(_module_dir):
        raise ImportError(
            f"shapes: expected the module from {_module_dir}, got {_loaded_from}"
        )


def pytest_report_header(config: pytest.Config) -> list[str]:
    return [f"shapes {shapes.__version__}: {shapes.__file__}"]


@pytest.fixture
def module_env() -> dict[str, str]:
    """Environment for a child Python, which imports the same `shapes`."""
    env = dict(os.environ)
    module_dir = str(Path(shapes.__file__).resolve().parent.parent)
    current = env.get("PYTHONPATH")
    env["PYTHONPATH"] = (
        module_dir if not current else os.pathsep.join([module_dir, current])
    )
    return env


# ---------------------------------------------------------------------------
# Building models in this process
#
# Groups and sinks cannot be created from Python; the only way to get them is
# to register a model with `library.model` and call `library.build_models`.
# The library is a single static of the module, so it is cleared after every
# test: a failed model would otherwise stay registered and run again on every
# later `build_models`.


@pytest.fixture(autouse=True)
def clean_library() -> Iterator[None]:
    previous_dir = shapes.library.output_directory
    previous_exporter = shapes.library.exporter_id
    yield
    shapes.library.clear_models()
    shapes.library.output_directory = previous_dir
    shapes.library.exporter_id = previous_exporter


_MESH_COMMENT = re.compile(r"^# Mesh '(?P<name>[^']*)' with (?P<faces>\d+) faces$")
_model_ids = itertools.count()


def read_obj_meshes(path: Path) -> dict[str, int]:
    """Face counts of an OBJ file written by Assimp, by object path."""
    meshes: dict[str, int] = {}
    for line in path.read_text(encoding="UTF-8").splitlines():
        match = _MESH_COMMENT.match(line)
        if match:
            name = match.group("name")
            meshes[name] = meshes.get(name, 0) + int(match.group("faces"))
    return meshes


BuildModel = Callable[[Callable[[shapes.group], None]], dict[str, int]]


@pytest.fixture
def build_model(tmp_path: Path) -> BuildModel:
    """Builds a model filled by the given function and exports it to OBJ;
    returns the face counts by object path. Exceptions raised while building
    pass through."""

    def build(fill: Callable[[shapes.group], None]) -> dict[str, int]:
        name = f"pytest_model_{next(_model_ids)}"

        def model(root: shapes.group):
            fill(root)
            return "obj"

        model.__name__ = name
        shapes.library.model(model)
        shapes.library.output_directory = tmp_path
        try:
            paths = shapes.library.build_models()
        finally:
            # a failed model stays registered; the next build starts clean
            shapes.library.clear_models()
        assert [path.name for path in paths] == [f"{name}.obj"]
        return read_obj_meshes(paths[0])

    return build


# ---------------------------------------------------------------------------
# Running code in a child interpreter
#
# For code which may crash the interpreter, if the bindings regress: a crash
# here would take the whole session, and its JUnit report, with it.

PRELUDE = """\
import gc
import shapes
from shapes import cm, library, solid

def cube():
    return solid.cube(cm(1), cm(1), cm(1))
"""

RunPython = Callable[[str], subprocess.CompletedProcess[str]]


@pytest.fixture
def run_python(module_env: dict[str, str], tmp_path: Path) -> RunPython:
    """Runs PRELUDE and the given source in a new interpreter, in tmp_path."""

    def run(source: str) -> subprocess.CompletedProcess[str]:
        script = tmp_path / "script.py"
        script.write_text(PRELUDE + textwrap.dedent(source), encoding="UTF-8")
        return subprocess.run(
            [sys.executable, "-X", "faulthandler", str(script)],
            env=module_env,
            cwd=tmp_path,
            capture_output=True,
            text=True,
            timeout=60,
        )

    return run


def describe(proc: subprocess.CompletedProcess[str]) -> str:
    """Exit code and output of a child, for assertion messages."""
    code = proc.returncode
    if code < 0 or code > 255:
        code_text = f"{code} (0x{code & 0xFFFFFFFF:08X}), a crash"
    else:
        code_text = str(code)
    return (
        f"exit code {code_text}\n--- stdout\n{proc.stdout}\n--- stderr\n{proc.stderr}"
    )


def assert_succeeded(proc: subprocess.CompletedProcess[str], stdout: str):
    assert proc.returncode == 0, describe(proc)
    assert proc.stdout.strip() == stdout, describe(proc)
