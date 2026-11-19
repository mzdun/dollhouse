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

- [ ] Write the spec of the model extension for interactions
  - [ ] Active objects and their names
  - [ ] States and the starting state
  - [ ] Actions and what triggers them
  - [ ] Input and output slots, with their types
  - [ ] Variables
  - [ ] How states drive the meshes (transforms, visibility, materials)
  - [ ] Storage of the glTF extension, next to `PYXEL_lights_*`
- [ ] Update [plans-schema.md](plans-schema.md) to link the new spec instead of the "draft available soon" note
- [ ] Declare active objects, states, actions, slots and variables in `pyxel-models`
- [ ] Expose them in the `shapes` Python module
- [ ] Export them to glb in `pyxel-io`
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

- [ ] Bundle `shapes` binaries for Python 3.12, 3.13, 3.14 and possibly 3.15 into one archive, once `shapes` has users other than its author
- [ ] Evaluate corefinement libraries (Manifold, CGAL, mcut) to replace the quadratic BSP in solid operations
  - [ ] Speed on dense solids
  - [ ] License compatible with MIT
  - [ ] Bit-identical results across builds
  - [ ] Available in Conan
- [ ] Python tests: add docstrings and factor the repeated model building into a `conftest.py` helper, then re-enable the `missing-*-docstring` and `duplicate-code` checks in `.github/linters/.python-lint`
- [ ] Range-checked unit conversions: `value_cast`, `round` and the converting constructor of `pyxel::math::value` have undefined behavior for NaN, infinity and out-of-range values when converting to integral storage (see the comment on [`detail::convert`](../libs/pyxel-models/include/pyxel/math/value.hpp#L65)); decide between throwing, clamping or a checked variant
