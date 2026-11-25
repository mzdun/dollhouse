# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""`library.model` while the interpreter shuts down, run in a child
interpreter: the models hold Python callables, which must not outlive it."""

import subprocess
import sys
import textwrap
from pathlib import Path

import pytest
from conftest import describe

REFUSED = "`late` is not registered, the interpreter is shutting down"

# runs after the atexit hook of shapes, registered by the import below it,
# as the hooks run last to first
ATEXIT_BEFORE_IMPORT = """
    import atexit

    def register_late():
        import shapes

        def late(root):
            pass

        print("registering", flush=True)
        shapes.library.model(late)

    atexit.register(register_late)
    import shapes
"""

# runs, when the module globals are cleared, after every atexit hook
FINALIZER = """
    import shapes

    class Late:
        def __init__(self):
            self.library = shapes.library

        def __del__(self):
            def late(root):
                pass

            print("registering", flush=True)
            self.library.model(late)

    kept = Late()
"""


@pytest.mark.parametrize(
    "source",
    [ATEXIT_BEFORE_IMPORT, FINALIZER],
    ids=["atexit-before-import", "finalizer"],
)
def test_model_registered_at_exit_is_refused(
    module_env: dict[str, str], tmp_path: Path, source: str
):
    script = tmp_path / "script.py"
    script.write_text(textwrap.dedent(source), encoding="UTF-8")
    proc = subprocess.run(
        [sys.executable, "-X", "faulthandler", str(script)],
        env=module_env,
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    # an exception in an atexit hook, or a finalizer, is only printed
    assert proc.returncode == 0, describe(proc)
    assert proc.stdout.strip() == "registering", describe(proc)
    assert REFUSED in proc.stderr, describe(proc)
