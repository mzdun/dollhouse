# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""`python -m shapes`, run in a child interpreter."""

import subprocess
import sys

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
