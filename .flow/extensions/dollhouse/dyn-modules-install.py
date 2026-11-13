# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""
Steps for the Python dynamic modules built by CMake:

- ``GenStubs`` generates the stubs of the modules with pybind11-stubgen,
  importing the modules from the build directory, and writes them next to
  the binaries;
- ``DynModulesInstall`` copies the binaries and their stubs into the
  site-packages of the interpreter running Flow.

Configuration (``.flow/config.yml``) names the modules:

.. code-block:: yaml

    python-dynamic-modules:
      - shapes

Each module is looked up in ``<build dir>/bin`` as either one of::

    NAME.<tag>.pyd            NAME/__init__.<tag>.pyd

where ``<tag>`` is the OS/ABI part, e.g. ``cp312-win_amd64``; its stubs,
as either one of::

    NAME.pyi                  NAME/**/*.pyi
"""

import os
import re
import subprocess
import sys
import sysconfig
from pathlib import Path

from proj_flow.api import env, step

EXTENSIONS = ["pyd", "so"]


def _modules(rt: env.Runtime) -> list[str]:
    return [str(name) for name in rt._cfg.get("python-dynamic-modules", [])]


def _bin_dir(config: env.Config) -> Path:
    return (config.build_dir / "bin").absolute()


def _site_packages() -> Path:
    return Path(sysconfig.get_path("platlib"))


def _locate_binaries(bin_dir: Path, module: str) -> list[Path]:
    """Paths, relative to bin_dir, of every built variant of the module."""
    patterns = [
        "{module}.*.{ext}",
        "{module}/__init__.*.{ext}",
    ]
    found = set[Path]()
    for pattern in patterns:
        for ext in EXTENSIONS:
            for path in bin_dir.glob(pattern.format(module=module, ext=ext)):
                found.add(path.relative_to(bin_dir))
    return sorted(found)


def _locate_stubs(root: Path, module: str) -> list[Path]:
    """Stubs of the module under root: ``<module>.pyi`` and
    ``<module>/**/*.pyi``."""
    result = list[Path]()
    file_name = root / f"{module}.pyi"
    dir_name = root / module

    if file_name.is_file():
        result.append(file_name)

    if dir_name.is_dir():
        result.extend(
            sorted(path for path in dir_name.rglob("*.pyi") if path.is_file())
        )

    return result


def _remove_stubs(rt: env.Runtime, root: Path, module: str):
    """Removes the stubs of the module from root, so the stubs of removed
    submodules do not linger."""
    for stub in _locate_stubs(root, module):
        rt.print("rm", str(stub))
        if not rt.dry_run:
            stub.unlink()


def _module_env(bin_dir: Path) -> dict[str, str]:
    """Environment for a child Python, which imports the modules from bin_dir
    (as the tests do), not from site-packages."""
    child_env = dict(os.environ)
    current = child_env.get("PYTHONPATH")
    child_env["PYTHONPATH"] = (
        str(bin_dir) if not current else os.pathsep.join([str(bin_dir), current])
    )
    return child_env


def _check_import(rt: env.Runtime, bin_dir: Path, module: str):
    """Fails, unless the child Python imports the module from bin_dir."""
    proc = subprocess.run(
        [sys.executable, "-c", f"import {module}; print({module}.__file__)"],
        env=_module_env(bin_dir),
        cwd=bin_dir,
        capture_output=True,
        encoding="UTF-8",
    )
    if proc.returncode != 0:
        rt.message(proc.stderr.rstrip(), level=env.Msg.ALWAYS)
        rt.fatal(f"gen-stubs: cannot import `{module}` from {bin_dir}")

    loaded_from = Path(proc.stdout.strip()).resolve()
    if not loaded_from.is_relative_to(bin_dir.resolve()):
        rt.fatal(
            f"gen-stubs: expected `{module}` from {bin_dir}, got {loaded_from}",
        )


def _copy_binary(rt: env.Runtime, dst: Path, so: Path):
    if so.is_symlink():
        real_so = (so.parent / so.readlink()).absolute()
        real_so_name = real_so.name
        result = rt.cp(str(real_so), str(dst / real_so_name))
        if result:
            return result

        so_name = so.name
        link_path = dst / so_name
        rt.print("ln", "-s", real_so_name, str(link_path))
        if rt.dry_run:
            return 0

        try:
            link_path.unlink(missing_ok=True)
            link_path.symlink_to(real_so_name)
        except Exception as e:
            rt.message("Error when linking:", repr(e), level=env.Msg.ALWAYS)
            return 1

        return 0

    return rt.cp(str(so), str(dst))


# pybind11-stubgen types some values (e.g. static properties) after their
# runtime class, which in case of std::filesystem::path is the concrete,
# platform-specific flavor of a path
_CONCRETE_PATH = re.compile(r"\bpathlib\.(?:Windows|Posix)Path\b")


def _patch_pathlib_Path(root: Path, module: str):
    """Replaces the platform-specific paths in the stubs of the module with
    ``pathlib.Path``, so the stubs are the same on every platform."""
    for stub in _locate_stubs(root, module):
        # newline="" keeps the line endings stubgen wrote
        with open(stub, encoding="UTF-8", newline="") as file:
            text = file.read()
        patched = _CONCRETE_PATH.sub("pathlib.Path", text)
        if patched == text:
            continue
        with open(stub, "w", encoding="UTF-8", newline="") as file:
            file.write(patched)


@step.register
class GenStubsStep:
    """
    Generates the stubs of the configured Python dynamic modules with
    pybind11-stubgen, importing the modules from the build directory, in any
    build configuration; writes them next to the binaries.
    """

    name = "GenStubs"
    runs_after = ["Build"]

    def platform_dependencies(self):
        return ["python -m pybind11-stubgen"]

    def is_active(self, config: env.Config, rt: env.Runtime) -> bool:
        return len(_modules(rt)) > 0

    def run(self, config: env.Config, rt: env.Runtime) -> int:
        bin_dir = _bin_dir(config)

        for module in _modules(rt):
            if not rt.dry_run:
                if not _locate_binaries(bin_dir, module):
                    rt.fatal(f"gen-stubs: `{module}` not found in {bin_dir}")
                _check_import(rt, bin_dir, module)

            _remove_stubs(rt, bin_dir, module)

            args = [
                sys.executable,
                "-m",
                "pybind11_stubgen",
                module,
                "-o",
                str(bin_dir),
            ]
            rt.print(*args)
            if rt.dry_run:
                continue

            proc = subprocess.run(args, env=_module_env(bin_dir), cwd=bin_dir)
            if proc.returncode != 0:
                rt.fatal(f"gen-stubs: pybind11-stubgen failed for `{module}`")

            _patch_pathlib_Path(bin_dir, module)
            if not _locate_stubs(bin_dir, module):
                rt.fatal(f"gen-stubs: no stubs written for `{module}` in {bin_dir}")

        return 0


@step.register
class DynModulesInstallStep:
    """
    Copies the configured Python dynamic modules, and their stubs made by
    GenStubs, to the site-packages of the current interpreter.
    """

    name = "DynModulesInstall"
    runs_after = ["Build", "GenStubs"]

    def is_active(self, config: env.Config, rt: env.Runtime) -> bool:
        return len(_modules(rt)) > 0

    def run(self, config: env.Config, rt: env.Runtime) -> int:
        site_packages = _site_packages()
        bin_dir = _bin_dir(config)

        for module in _modules(rt):
            files = _locate_binaries(bin_dir, module)
            if not files and not rt.dry_run:
                rt.fatal(
                    f"dyn-modules-install: `{module}` not found in {bin_dir}",
                )

            stubs = _locate_stubs(bin_dir, module)
            if not stubs and not rt.dry_run:
                rt.fatal(
                    f"dyn-modules-install: no stubs of `{module}` in {bin_dir}; "
                    "run the GenStubs step first",
                )

            for dll in files:
                dst_dir = (site_packages / dll).parent
                result = rt.mkdirs(str(dst_dir))
                if result:
                    return result
                result = _copy_binary(rt, dst_dir, bin_dir / dll)
                if result:
                    return result

            _remove_stubs(rt, site_packages, module)
            for stub in stubs:
                dst = site_packages / stub.relative_to(bin_dir)
                result = rt.mkdirs(str(dst.parent))
                if result:
                    return result
                result = rt.cp(str(stub), str(dst))
                if result:
                    return result

        return 0
