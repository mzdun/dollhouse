# Roadmap

Each stage ends with something that runs. Stage 3 (Plans) does not depend on the Inspector and can be worked on in parallel with stages 1 and 2.

## Done so far

- [x] Set up Dollhouse structure
- [x] Set up `pybind11` module for `shapes` exposing `pyxel-models`
- [x] Implement `pyxel-models` up to and including groups
- [x] Simple cube
- [x] Add proper diffusion materials (paints and glass/metal)
- [x] Solid operations
- [x] Export models with diffusion materials
- [x] Add random number generator with seeding
- [x] Export light sources
- [x] Propose YAML schema for plans

## Stage 0: interactions in models

- [x] Write the [spec](model-interactions.md) of the model extension for interactions
  - [x] Active objects and their names
  - [x] States and the starting state
  - [x] Actions and what triggers them
  - [x] Input and output slots, with their types
  - [x] Variables
  - [x] How states drive the meshes: transforms; visibility (the reserved `visible`) and materials are left to a later version
  - [x] Storage in the glTF file: the `PYXEL_interactions` extra of the root node, written after Assimp, next to `PYXEL_lights_area`
  - [x] JSON Schema of the extra: [interactions.schema.json](../schemas/interactions.schema.json)
- [x] Update [plans-schema.md](plans-schema.md) to link the new spec instead of the "draft available soon" note
- [ ] Post-process every glTF 2 export, not only the binary one, with what Assimp cannot write
  - [x] `gltf2-lights`: the lights for `gltf2`, as `glb2-lights` adds them for `glb2`
  - [x] Rename both to `glb2-pyxel` and `gltf2-pyxel`, taking the place of `glb2` and `gltf2`
  - [ ] Add the `PYXEL_interactions` extra to both pyxel exporters; each adds everything of ours to the glTF document: `KHR_lights_punctual`, `PYXEL_lights_area` and the `PYXEL_interactions` extra
- [x] Read the area lights back: a glTF 2 importer in `pyxel-io`, taking the place of Assimp's, adds the `PYXEL_lights_area` lights to the scene
- [x] Declare active objects, states, actions, slots and variables in `pyxel-models`
- [x] Expose them in the `shapes` Python module
- [x] Export them to glb and glTF in `pyxel-io`, through `glb2-pyxel` and `gltf2-pyxel`
- [ ] Provide rules API for transition runner
  - [ ] Parse the rules
  - [ ] Diverge for light active objects and for geometry-based active objects
  - [ ] Propagate `"*"` (with target type filtering, on lights paths assume `"light"` refers to a slot)
  - [ ] For lights, propagate `all` with used channels filtering, using `set` as the only supported channel.
  - [ ] Take the `set` rule for `lite` and store it for usage with lights.
  - [ ] For geo objects, propagate `all` with used channels filtering.
  - [ ] Build lookup table of parsed rules per target.
  - [ ] Gather all group rules in a lookup table and all slots in a separate lookup table; for lights put `"light"` into slots.
  - [ ] Store both lookup tables with loaded active object
- [ ] Fix "Order within a tick" in the spec: step 5 registers only the changes made in steps 1 and 4, so a state change that takes no time (it stops at once in step 2 or 3) never sends its new value along the wires; register every output and variable whose value changed anywhere in steps 1 to 4. Resolve before Dollhouse work (stage 4) starts.
- [ ] A CLI tool for quick checks of models: read a `.glb` or `.gltf` file and print its bounding box and active objects, with their states, actions, slots, variables and transitions
- [ ] Sample models: a door, a light switch, a lamp

## Stage 1: Inspector as a viewer

- [ ] Application framework
  - [ ] Input binding engine (buttons, single-axis wheels, two-axis sticks), designed with controllers in mind
  - [ ] Mouse and keyboard
  - [ ] Keep the window state between runs
- [ ] GLFW backend in `libs/backends/glfw`
  - [ ] Window, graphics context and the main loop
  - [ ] Mouse and keyboard events fed to the input binding engine
  - [ ] Window position, size and state, for keeping them between runs
  - [ ] Verify the application framework against it: an empty Inspector window that opens, takes input, closes and reopens where it was left
- [ ] UI engine (independent of the backend window implementation)
  - [ ] Log view (warnings, errors)
  - [ ] Controls for the sun
- [ ] Model loader/reloader, with a file watcher (the model and the files it depends on, such as the `.bin` of a `.gltf`)
- [ ] Simplest shaders, with diffusion materials
- [ ] Back-face culling and depth test
- [ ] Orbit camera around the model
- [ ] Fallback sky box (the only one in Inspector)
- [ ] Light from the sun, without shadows
- [ ] Light sources from the model, without shadows

## Stage 2: Inspector as a tester

Needs stage 0.

- [ ] Load active objects, states, actions, slots and variables from the glb
- [ ] Model checks (active objects, slots, slot types), reported in the log view
- [ ] Runtime for states, actions and slots
- [ ] UI
  - [ ] Controls for states and actions
  - [ ] Controls for inputs
  - [ ] View of outputs
- [ ] Interactions
  - [ ] Pick the active object under the mouse pointer
  - [ ] Highlight the mesh of the picked object
  - [ ] Trigger the picked object's action (click, controller button or wheel; TBD)

## Stage 3: Plans geometry

Independent of stages 1 and 2; a library, tested without UI.

- [ ] Read the project: `include`, combining files, named entries and single values
- [ ] Project verifier, errors: JSON Schema plus project-wide checks (names, references, wall cycles), from the project files alone
- [ ] Place the walls on the plan (measurements, walls starting from other walls, inferred direction and side)
- [ ] Build the floor plan: split it into rooms, including invisible walls
- [ ] Build the walls: merged walls on one line, lower and elevated walls
- [ ] Cut the openings, with doorways and thresholds
- [ ] Synthesize nodes for openings with models
- [ ] Assign faces to rooms and walls
- [ ] Paint materials, with room defaults and face overrides
- [ ] Merge touching coplanar faces with the same material
- [ ] Read the sky box and the sun
- [ ] Support `transform` on YAML nodes
- [ ] Hand the house over to the viewer (decide: glb through `pyxel-io`, or in memory), keeping rooms and the openings between them for portal culling

## Stage 4: Dollhouse MVP

- [ ] Reuse the Inspector framework (app and UI)
- [ ] (Re)load project
- [ ] Project observer, reusing the Inspector file watcher (reload when any file of the project changes), watching:
  - the main project file
  - includes
  - textures and bump maps
  - sky box pictures
  - models and the files they depend on (reused from Inspector)
- [ ] Show the house built by Plans
- [ ] Frustum culling of rooms and nodes
- [ ] Place nodes with external models, in their starting states
- [ ] Spectator camera
  - [ ] Jump between the project cameras
- [ ] Pick the active object under the crosshair
- [ ] Wire nodes together
- [ ] Project verifier, hard warnings: read the external models and check model files, active objects, slots and slot types
- [ ] Show errors and warnings in the log view
- [ ] Extend the window state with per-project session data: slot values, variables, the last selected camera (if any), the current camera type and its setup

## Stage 5: looks

- [ ] Shadows from the sun
- [ ] Shadows from the model lights
  - [ ] Spot
  - [ ] Point
- [ ] Use the sun and sky box from the project
  - [ ] `cube` (six pictures)
  - [ ] `cross` (one unfolded cube)
  - [ ] `panorama` (equirectangular)
  - [ ] `hdri` (HDR panorama), with the sun found in the picture
- [ ] Build plinths and moldings
- [ ] First-person camera

## Stage 6: textures

- [ ] Solid face identification (for textures)
- [ ] UV mapping/unwrapping
  - [ ] Automatic for floor and wall tiling
  - [ ] Automatic for foil on furniture boards
  - [ ] By hand
- [ ] Export texture materials without bumps and normals
- [ ] Export texture materials with bumps and normals
- [ ] Textures in the shaders (Inspector and Dollhouse)
- [ ] Tiling materials in Plans, with the pattern continued across neighboring faces

## Stage 7: polish

- [ ] Wires overlay (à la Scrap Mechanic), with markers for problems in the models
- [ ] Camera overlay (toggleable frustums, à la Blender)
- [ ] Xbox and PlayStation 5 controllers (detect the type, map Xbox A to PlayStation Cross)
- [ ] Strict mode, like `-E w-error=on`, turning hard warnings into errors
- [ ] Shadows from area lights

## Stage 8: performance

Driven by measurements, not done up front.

- [ ] Profiling: frame time, draw calls, triangles, on a large sample house
- [ ] Portal culling: from the camera's room, draw only the rooms seen through the openings
- [ ] Instancing of repeated models
- [ ] Fewer draw calls for the static geometry of the house

## Any time

- [ ] Prepare free-threaded Python module; until then, `shapes` declares itself GIL-free only with `-E freethreaded-shapes=on` (`DOLLHOUSE_FREE_THREADED`), so a free-threaded interpreter turns the GIL on when importing it
  - [ ] Set up a version of environment with 3.14t (`uv python install`)
  - [ ] Avoid per-thread `library`, output directory on one thread _needs_ to be used in `build_models()` on another
  - [ ] Put a `mutable std::mutex m;` on each modifier.
    - [ ] Release it before calling a model function: the model calls back into the module (`register_paint`, nested `library` calls)
  - [ ] For main, prepare better library separation.
  - [ ] Scope stack of the sinks (`python_scopes()`, with its lease ids) per thread: unlike the library, a scope belongs to an `output` call, which runs on one thread
  - [ ] `detach_wrappers` under the locks of pybind11 (`with_instance_map`) or `py::scoped_critical_section`
  - [x] Critical sections on the mutable declarations of active objects (`state`, `transitions`) and on the active objects of a `group`
  - [ ] Check the global state of `pyxel-models` and `pyxel-io` (paint registry, exporter list, Assimp)
  - [ ] Tests with several threads building models and outputting objects, run on 3.14t in CI
  - [ ] Turn `DOLLHOUSE_FREE_THREADED` on by default, or drop the option
- [ ] Bundle `shapes` binaries for Python 3.12, 3.13, 3.14 and possibly 3.15 into one archive, once `shapes` has users other than its author
- [ ] Evaluate corefinement libraries (Manifold, CGAL, mcut) to replace the quadratic BSP in solid operations
  - [ ] Speed on dense solids
  - [ ] License compatible with MIT
  - [ ] Bit-identical results across builds
  - [ ] Available in Conan
- [ ] Python tests: add docstrings and factor the repeated model building into a `conftest.py` helper, then re-enable the `missing-*-docstring` and `duplicate-code` checks in `.github/linters/.python-lint`
- [ ] Model names: reject `\0`, `:` and the reserved names of Windows (`CON`, `NUL`, ...), which still pass the check of the output directory
- [ ] `python -m shapes` started in the scripts directory without `-d` still has it as `sys.path[0]` (set by Python itself); document `python -P -m shapes`, or `PYTHONSAFEPATH=1`
- [ ] Packages and the variant file of `shapes`
  - [ ] Install the `.pyi` stubs with the module (CPack)
  - [ ] Leave the MSVC import library (`shapes.lib`) out of the packages
  - [ ] `dirty` in `__init__.variant.json` ignores the untracked files (`git status --porcelain --untracked-files=no`)
  - [ ] Expose the variant to the module users (`shapes.__variant__`)
- [ ] Add `pyyaml` to the Python environment of the tests, so `test_listing_is_yaml` is not skipped
- [ ] Range-checked unit conversions: `value_cast`, `round` and the converting constructor of `pyxel::math::value` have undefined behavior for NaN, infinity and out-of-range values when converting to integral storage (see the comment on [`detail::convert`](../libs/pyxel-models/include/pyxel/math/value.hpp#L65)); decide between throwing, clamping or a checked variant
- [ ] Wires within one model, still in version 1.0 of the interactions: a bedside lamp with a switch on it wires the `enabled` output of its switch to the bulb inside it in the model itself, not in the floor plan
- [ ] Several switches for one light, still in version 1.0 of the interactions: find a way to model two-way (staircase) and intermediate (cross) switches, where flipping any of them toggles the light
