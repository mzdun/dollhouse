# Planner

## Critical Rules

- Never commit
- Before any other Flow command on a new environment bootstrapping is needed. It will install all dependencies, including PyYAML.
- After bootstrapping there will by a `.venv/`. Use it for all Python calls.
- Configure, build, test and run through `python3 ./.flow/flow.py`, not directly
- Package configuration is needed, if `conanfile.txt` was modified, or if `build/conan/` is missing. Otherwise it should not be called.
- Buildsystem generation is needed if binary directory is missing (such as `build/debug` or `build/release`)

## Build system layout

- All the details for a given build is taken from `.flow\matrix.yml`, a GitHub-style policy matrix
- Matrix is limited through one of the keys to `"shortcuts"` object in `.flow\config.yml`. Shortcuts are mostly used as double-dashed flags, e.g. `--dbg` would select Debug build type with no code sanitization.
- Package dependencies go to `build/conan/`
- Development builds go to `build/debug/`
- Release builds go to `build/release/`
- After build, `shapes` Python dynamic module is copied to current Python installation

## Commands

- Package manager: `conan` 2.0 or later
- Buildsystem generator: `cmake` 4.3 or later
- Bootstrap: `python3 ./.flow/flow.py bootstrap`
- Configure packages: `python3 ./.flow/flow.py run --${shortcut} -s conan`
- Buildsystem generation: `python3 ./.flow/flow.py run --${shortcut} -s cmake`
- Build: `python3 ./.flow/flow.py build --${shortcut}`
- Dynamic module install: `python3 ./.flow/flow.py dyn-install --${shortcut}`
