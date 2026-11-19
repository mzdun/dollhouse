# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""`python -m shapes`, run in a child interpreter."""

import subprocess
import sys
import textwrap
from pathlib import Path

import pytest
import shapes


def run_shapes(env: dict[str, str], *args: str):
    return subprocess.run(
        [sys.executable, "-m", "shapes", *args],
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )


def test_version(module_env: dict[str, str]):
    proc = run_shapes(module_env, "--version")
    assert proc.returncode == 0, proc.stderr
    assert proc.stdout.strip() == f"shapes {shapes.__version__}"


def test_unknown_option(module_env: dict[str, str]):
    proc = run_shapes(module_env, "--no-such-option")
    assert proc.returncode == 2
    assert "unrecognized argument" in proc.stderr


# ---------------------------------------------------------------------------
# Finding and loading the scripts

MODEL = """\
from shapes import cm, library, solid


@library.model
def {name}(root):
    root.add(solid.cube(cm(1), cm(1), cm(1)))
"""


def write(directory: Path, filename: str, source: str) -> Path:
    path = directory / filename
    path.write_text(textwrap.dedent(source), encoding="UTF-8")
    return path


@pytest.fixture(name="scripts")
def fixture_scripts(tmp_path: Path) -> Path:
    directory = tmp_path / "scripts"
    directory.mkdir()
    return directory


def run_in(env: dict[str, str], tmp_path: Path, scripts: Path, *names: str):
    """Runs the tool on `scripts` from tmp_path, writing to tmp_path/out."""
    return subprocess.run(
        [
            sys.executable,
            "-m",
            "shapes",
            "-d",
            str(scripts),
            "-o",
            str(tmp_path / "out"),
            *names,
        ],
        env=env,
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )


def test_default_runs_model_scripts_only(module_env, tmp_path, scripts):
    write(scripts, "table.model.py", "print('table')\n" + MODEL.format(name="table"))
    write(scripts, "chair.model.py", "print('chair')\n" + MODEL.format(name="chair"))
    write(scripts, "notes.py", "print('notes')\n")
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 0, proc.stderr
    # sorted by name, the helper is never run
    assert proc.stdout.split() == ["chair", "table"]
    assert sorted(p.name for p in (tmp_path / "out").iterdir()) == [
        "chair.glb",
        "table.glb",
    ]


def test_named_script(module_env, tmp_path, scripts):
    write(scripts, "chair.model.py", MODEL.format(name="chair"))
    write(scripts, "table.model.py", MODEL.format(name="table"))
    write(scripts, "stool.py", MODEL.format(name="stool"))
    proc = run_in(module_env, tmp_path, scripts, "chair", "stool.py", "table.model.py")
    assert proc.returncode == 0, proc.stderr
    assert sorted(p.name for p in (tmp_path / "out").iterdir()) == [
        "chair.glb",
        "stool.glb",
        "table.glb",
    ]


def test_script_named_like_a_loaded_module(module_env, tmp_path, scripts):
    # `abc` is imported by the interpreter itself; the script still runs
    write(scripts, "abc.model.py", MODEL.format(name="box"))
    write(scripts, "os.py", MODEL.format(name="other"))
    proc = run_in(module_env, tmp_path, scripts, "abc", "os")
    assert proc.returncode == 0, proc.stderr
    assert sorted(p.name for p in (tmp_path / "out").iterdir()) == [
        "box.glb",
        "other.glb",
    ]


def test_helper_does_not_shadow_stdlib(module_env, tmp_path, scripts):
    write(scripts, "json.py", "print('planted json.py')\n")
    write(scripts, "chair.model.py", "import json\n" + MODEL.format(name="chair"))
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 0, proc.stderr
    assert "planted" not in proc.stdout


def test_scripts_import_their_helpers(module_env, tmp_path, scripts):
    write(scripts, "helper.py", "SIDE = 2\n")
    write(
        scripts,
        "chair.model.py",
        """\
        import sys
        from helper import SIDE
        from shapes import cm, library, solid

        print(__name__, sys.path.count(sys.path[-1]))


        @library.model
        def chair(root):
            import helper  # pylint: disable=import-outside-toplevel

            root.add(solid.cube(cm(helper.SIDE), cm(SIDE), cm(SIDE)))
        """,
    )
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 0, proc.stderr
    assert proc.stdout.split() == ["shapes:chair", "1"]
    assert (tmp_path / "out" / "chair.glb").is_file()


def test_search_path_is_restored(module_env, tmp_path, scripts):
    # the script directory is on sys.path only while its script runs
    write(
        scripts,
        "a.model.py",
        "import sys\nprint(sys.path[-1])\n" + MODEL.format(name="a"),
    )
    write(
        scripts,
        "b.model.py",
        "import sys\nprint(sys.path.count(sys.path[-1]))\n" + MODEL.format(name="b"),
    )
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 0, proc.stderr
    directory, count = proc.stdout.split()
    assert Path(directory) == scripts.resolve()
    assert count == "1"


def test_script_without_models(module_env, tmp_path, scripts):
    write(scripts, "empty.model.py", "pass\n")
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 1
    assert "registered no models" in proc.stderr


def test_unknown_script(module_env, tmp_path, scripts):
    proc = run_in(module_env, tmp_path, scripts, "missing")
    assert proc.returncode == 2
    assert "script `missing` not found" in proc.stderr


def test_no_model_scripts(module_env, tmp_path, scripts):
    write(scripts, "chair.py", MODEL.format(name="chair"))
    proc = run_in(module_env, tmp_path, scripts)
    assert proc.returncode == 2
    assert "no `*.model.py` scripts" in proc.stderr
