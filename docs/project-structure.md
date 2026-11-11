# Dollhouse

## Pyxel models

- uses `pyxel` namespace
- contains 3D graphics math
  - based on glm
  - uses strong typedefs for angles, lengths, offsets and sizes, inspired by std::chrono
- allows watching file changes to notify the upper layers to refresh their logic
- defines 3D objects, some anonymous, some named; named object may have event handling (mostly: activate). Think light switches, doors opening/closing.
- defines materials: paints, textures, allows wrapping a texture on a group
- defines directional, spot and point lights
- uses assimp to load/store objects

## Pyxel engine

- uses `pyxel` namespace
- depends on `pyxel-models`
- defines cameras (free-flying or player-bound), windows, I/O (keyboard, mouse, gamepad), etc.
- defines 3D objects, some anonymous, some named; named object may have event handling (mostly: activate). Think light switches, doors opening/closing
- enables object collisions to enable the player-bound movement
- defines textures and sky boxes

## GLFW backend

- uses `pyxel::glfw` namespace
- depends on `pyxel-engine`
- implements window, I/O, ... interface

## Vulcan backend

- probably never
- uses `pyxel::vulcan` namespace
- depends on `pyxel-engine`
- implements window, I/O, ... interface

## Shapes (python package)

- uses `shapes` namespace
- depends on `pyxel-models`
- a Python 3 package exposing `pyxel-models`
- is a tool (has a `__main__`), so it can be used directly
- batch and single mode, plus `--watch` mode

## Floor plans

- uses `plans` namespace
- depends on `pyxel-engine`
- main responsibility is to define the YAML parser and putting the whole scene together

## Dollhouse (app)

- uses `house` namespace
- depends on `floor-plans`
- selects the backend for engine
- runs the application
