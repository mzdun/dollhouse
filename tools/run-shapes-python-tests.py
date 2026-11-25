#!/usr/bin/env python3

# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""
Run the `shapes` Python bindings tests the way CTest does.

The command, environment and working directory are taken from the
``shapes-python`` test registered in ``build/<preset>`` (see
extras/shapes/CMakeLists.txt), so the run matches CTest: the same interpreter,
the module from that build tree, the JUnit report, and on sanitizer builds the
preloaded ASan runtime. Every argument this script does not know goes to
pytest:

    tools/run-shapes-python-tests.py
    tools/run-shapes-python-tests.py --preset release -k rng -x
    tools/run-shapes-python-tests.py test_solid.py::test_cube -v

Test paths are looked up in the current directory first, then in the test
directory, so ``test_solid.py`` works from anywhere.
"""

import argparse
import inspect
import json
import os
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST_DIR = ROOT / "extras" / "shapes" / "tests" / "python"
TEST_NAME = "shapes-python"
PRESETS = ("debug", "release")


class Error(Exception):
    pass


class _DocstringFormatter(argparse.RawDescriptionHelpFormatter):
    """
    Formats the help text paragraph by paragraph: prose is refilled to the
    console width, indented blocks (the examples) are kept as they are.
    """

    def _fill_text(self, text: str, width: int, indent: str) -> str:
        paragraphs = inspect.cleandoc(text).split("\n\n")
        return "\n\n".join(
            self._fill_paragraph(paragraph, width, indent) for paragraph in paragraphs
        )

    @staticmethod
    def _fill_paragraph(paragraph: str, width: int, indent: str) -> str:
        lines = paragraph.splitlines()
        if all(line.startswith((" ", "\t")) for line in lines):
            return "\n".join(indent + line.rstrip() for line in lines)
        return textwrap.fill(
            " ".join(line.strip() for line in lines),
            width,
            initial_indent=indent,
            subsequent_indent=indent,
            break_on_hyphens=False,
        )


def _parse_args() -> tuple[argparse.Namespace, list[str]]:
    description, _, epilogue = (__doc__ or "").partition("\n\n")
    parser = argparse.ArgumentParser(
        description=description,
        epilog=epilogue,
        formatter_class=_DocstringFormatter,
        allow_abbrev=False,
    )
    parser.add_argument(
        "--preset",
        choices=PRESETS,
        default="debug",
        help="build tree to test, build/<preset> (default: %(default)s)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="print the environment and command, do not run them",
    )
    return parser.parse_known_args()


def _ctest_definition(build_dir: Path) -> dict:
    """Reads the test definition from `ctest --show-only=json-v1`."""
    if not (build_dir / "CTestTestfile.cmake").is_file():
        raise Error(f"{build_dir} is not configured; run `./flow config` first")
    ctest = shutil.which("ctest")
    if ctest is None:
        raise Error("ctest not found on PATH")
    output = subprocess.run(
        [ctest, "--show-only=json-v1", "-R", f"^{TEST_NAME}$"],
        cwd=build_dir,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    for test in json.loads(output).get("tests", []):
        if test.get("name") == TEST_NAME:
            return test
    raise Error(f"{build_dir} has no {TEST_NAME} test (is DOLLHOUSE_TESTING on?)")


def _properties(test: dict) -> dict:
    return {prop["name"]: prop["value"] for prop in test.get("properties", [])}


def _resolve_path(arg: str) -> str | None:
    """Returns an absolute test path for `arg`, or None if it is not one."""
    path, sep, node = arg.partition("::")
    for candidate in (Path(path), TEST_DIR / path):
        if candidate.exists():
            return str(candidate.resolve()) + sep + node
    return None


def _command(test: dict, extra: list[str]) -> list[str]:
    command = list(test["command"])

    # pytest options may take values (`-k name`), so anything that is not an
    # existing path stays as it is
    paths: list[str] = []
    options: list[str] = []
    for arg in extra:
        resolved = None if arg.startswith("-") else _resolve_path(arg)
        if resolved is None:
            options.append(arg)
        else:
            paths.append(resolved)

    if paths and command and Path(command[-1]) == TEST_DIR:
        # the test directory is the last argument CTest passes; the same path
        # is also the value of --rootdir, which has to stay
        command.pop()
    return [*command, *options, *paths]


def _environment(test: dict) -> dict[str, str]:
    env = dict(os.environ)
    for entry in _properties(test).get("ENVIRONMENT", []):
        name, _, value = entry.partition("=")
        env[name] = value
    return env


def main() -> int:
    args, extra = _parse_args()
    build_dir = ROOT / "build" / args.preset

    try:
        test = _ctest_definition(build_dir)
    except Error as error:
        print(f"run-shapes-python-tests: {error}", file=sys.stderr)
        return 1

    module_dir = Path(
        _environment(test).get("SHAPES_MODULE_DIR", str(build_dir / "bin"))
    )
    if not any((module_dir / "shapes").glob("__init__*")):
        print(
            f"run-shapes-python-tests: no shapes module in {module_dir / 'shapes'}; "
            "build it first with `./flow build`",
            file=sys.stderr,
        )
        return 1

    command = _command(test, extra)
    cwd = _properties(test).get("WORKING_DIRECTORY", str(TEST_DIR))

    if args.dry_run:
        for entry in _properties(test).get("ENVIRONMENT", []):
            print(entry)
        print(f"cd {cwd}")
        print(" ".join(command))
        return 0

    return subprocess.call(command, env=_environment(test), cwd=cwd)


if __name__ == "__main__":
    sys.exit(main())
