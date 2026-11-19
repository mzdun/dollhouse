# Plans project schema

## General notes

Project files are written in YAML, JSON, or a mix of both. They are versioned (the current version is 1) and can be split into multiple files.

- **Includes.** Every file may have an `include` list of strings. It is read before any other property of the file: the included files are read in the order of the list, each together with its own includes, and only then the rest of the including file. Paths are resolved as described in **Paths** below. An included file that is missing or cannot be read is an error.
  - **Repeats.** A file included more than once, directly or through other files, is read again each time it is included. For example, when `a` has `include: [b, c]`, and both `b` and `c` have `include: [d]`, the files are read in the order `d`, `b`, `d`, `c`, `a`. The second reading of `d` replaces the entries of `b` that share a name with an entry of `d`, as described below. Its unnamed entries and its nodes are added a second time, so every unnamed node of `d` places its model twice in the same spot, and a named one is an error. A file read more than once therefore gives a [warning](#warnings).
  - **Cycles.** A file that includes itself, directly or through other files, would be read without end. Such a cycle is an error.
- **Names.** Walls, rooms, materials, models and cameras each have names of their own: a wall and a room may share a name without a conflict. The only exception are [nodes](#nodes) and [openings](#openings-doors-and-windows), which share one namespace, as every opening with a model is filled by a node of its own. A node may still share its name with a model; see the `exit door` in the [example of external models](#external-models).
- **Combining files.** The sections of all files, in the order the files are read, are combined into one project. Two entries are never merged property by property: one of them is kept as a whole.
  - **Named entries.** In `walls`, `rooms`, `materials`, `models` and `cameras`, an entry whose name already appeared earlier in the same section replaces the earlier entry as a whole, and takes its place in the list. This holds across files and within one file alike. A wall replaced this way takes its openings with it. Every such replacement gives a [warning](#warnings).
  - **Other entries.** An entry without a name, or with a name that has not appeared yet, is added at the end of its section.
  - **Nodes.** Every top-level node is a separate view into its room, so nodes are never replaced: the `nodes` lists of all files are joined one after another. Names of nodes and openings must still be unique in the whole project.
  - **Single values.** For the properties at the top of the file, each key of `defaults` and the `sky-box` section, the value read last is used as a whole.

  As every file is read after the files it includes, the including file wins over its includes, and the root file wins over all of them. The combined project is what is checked and built. Wherever this document speaks of the order of declaration, for example of the first camera, it means the order in the combined sections.
- **References.** A name that refers to another entry must name an existing one. A missing wall, room, model or wired node is an error. A model file that cannot be read, or that lacks the objects and slots a wiring expects, does not stop the house from being shown, but the viewer must say what will not work. Other files referred to by paths, such as textures and sky box pictures, are treated leniently: a missing one gives a warning. See the [Summary of error handling](#summary-of-error-handling) at the end of this document.
- **Paths.** Every path in the project, in `include`, `models`, materials and the sky box, is relative to the directory of the file that names it. A project written on one system must open on another, so:
  - both `/` and `\` separate the parts of a path on every system; a file or directory whose name contains a backslash therefore cannot be named in a project,
  - paths are case-sensitive, even on systems that ignore case,
  - an absolute path, such as `/x`, `C:\x`, `C:x` or `\\server\x`, or a URL is an error.

  When a file cannot be found, a viewer may check whether it would be found with the backslashes taken literally, as part of a name. The file still counts as missing, but the message can then say that its name contains a backslash and should be changed.
- **Directories.** When a project is read from a directory, the root file is the first of these that exists: `<name>.house` (if it is the only `.house` file in the directory), `index.house`, `index.yaml`, `index.yml` or `index.json`. Its `include` list pulls in the rest of the project.

### Scope of version 1

Version 1 of the format describes only single-storey flats with a constant ceiling height:

- **One storey.** The whole project is a single floor plan, with one floor at elevation 0 and one ceiling above it. There are no staircases, mezzanines or floors stacked one above another.
- **Flat ceiling.** The ceiling has the same height everywhere, given by the `wall-height` [default](#defaults). Rooms cannot set a ceiling height of their own, and sloped ceilings, such as those under a roof, cannot be described.

Later versions of the format may lift these limits.

### Top of the file

A project file starts with a few general properties, followed by the sections describing the house.

```yaml
$schema: https://raw.githubusercontent.com/mzdun/dollhouse/main/schemas/house.schema.json
version: 1
title: <model title>
north: -10
defaults:
  wall-width: 8
  wall-height: 248
  materials:
    floor: gray floor covering
    walls: mint paint
    ceiling: white paint
include: []
walls: []
# etc
```

- `$schema` points to the JSON Schema of the format, so that editors can validate and complete the file.
- `version` is the version of the format. The current version is 1. It must be given in at least one file of the project, preferably the root file, such as `index.yaml`; in the other files it is optional. All files of a project are assumed to be compatible with each other. The [JSON Schema](https://raw.githubusercontent.com/mzdun/dollhouse/main/schemas/house.schema.json) validates single files, so it cannot check this rule and treats `version` as optional.
- `title` _(optional)_ is the title of the model. Without it, what the title bar shows is up to the viewer.
- `north` _(optional)_ is the direction of north on the plan, in degrees. A viewer may use it to show a compass, and it turns the [sun](#sun) together with the plan. Positive and negative angles are explained in [Placement on the plan](#placement-on-the-plan). It defaults to -90, so that north is up on the screen, as on most maps.
- `defaults` lists the values other parts of the project fall back on; see [Defaults](#defaults) below.
- `include` lists the other files of the project; see **Includes** above.
- `walls` and the properties after it hold the sections listed in [Contents](#contents).

### Defaults

| Property | Meaning |
| -------- | ------- |
| `wall-width` | Width of a wall that does not set its own, in centimeters _(required)_. |
| `wall-height` | Height of a wall that does not set its own, and the height of the ceiling, in centimeters _(required)_. |
| `materials.floor` | Material of a floor face that no room or override sets. |
| `materials.walls` | Material of a wall face that no room or override sets. |
| `materials.ceiling` | Material of a ceiling face that no room or override sets. |
| `plinth` | [Plinth](#plinths-and-moldings) of a room that does not set its own. |
| `molding` | [Molding](#plinths-and-moldings) of a room that does not set its own. |

The `materials` map has the same `floor`, `walls` and `ceiling` keys as the [materials of a room](#materials). Like every other key of `defaults`, it is replaced as a whole when [several files](#general-notes) define it.

As with `version`, `wall-width` and `wall-height` are required in the project, in at least one of its files, preferably the root file. The JSON Schema cannot check this and treats them as optional.

### Contents

The project files describe:

- [Walls](#walls-and-rooms), the boundaries of the house.
- [Rooms](#rooms), named areas between the walls, with the look of their faces.
- [Openings](#openings-doors-and-windows), such as doors and windows, cut in the walls.
- [Materials](#room-materials) covering the faces of the house.
- [Models](#external-models) of doors, windows, furniture, lamps, fixtures and so on.
- [Nodes](#nodes) placing the external models in the house.
- [Cameras](#cameras) for quick navigation.
- [Sky box](#sky-box), including the sun.

## Coordinates on the plan

Because walls are reduced to two-dimensional lines, the plan uses two-dimensional coordinates, oriented like a computer screen:

- The origin is in the top-left corner.
- The X axis grows to the right.
- The Y axis grows downwards.

An entity that sits above floor level declares its **elevation**. X, Y and elevation are all measured in centimeters. In transforms, the vertical axis, perpendicular to the plan, is called `vert`.

### Ranges of distances

Lengths, widths, heights and sizes are larger than 0. Offsets and translations may be negative. A value outside its range does not match the [JSON Schema](https://raw.githubusercontent.com/mzdun/dollhouse/main/schemas/house.schema.json), which is an [error](#errors).

| Property | Where | Range |
| -------- | ----- | ----- |
| `length`, `width`, `height` | [wall](#wall-properties) | larger than 0 |
| `wall-width`, `wall-height` | [`defaults`](#defaults) | larger than 0 |
| `size`, both values | [opening](#opening-properties), [tiles](#tiles) | larger than 0 |
| `elevation` | wall, opening | 0 or more |
| `grout.width`, `grout.depth` | tiles | 0 or more |
| points of a `profile`, both values | [plinth, molding](#plinths-and-moldings) | 0 or more |
| `fovy` | [camera](#cameras) | between 0 and 180, both excluded |
| `offset` | wall `origin`, opening | any |
| `offset.x`, `offset.y` | tiles | any |
| `offset` | camera | any |
| `elevation` | camera | any, see [Views from outside](#views-from-outside) |
| `translate`, `translate.x`, `translate.y`, `translate.vert` | [node](#translate) | any |
| `scale`, `scale.x`, `scale.y`, `scale.vert` | [node](#scale) | any, including 0, which flattens the model; like a negative scale, this is the author's decision |

## Model space

3D models are written in the ordinary three-dimensional **model space**:

- The X axis points right.
- The Y axis points up.
- The Z axis points toward the camera, out of the model's front.
- Positive rotations follow the right-hand rule: looking from the positive end of the axis toward the origin, they turn counterclockwise.

The plan is converted to model space when the walls, floors and ceilings are built into the 3D model, and external models are placed on the plan the same way. The plan's X stays X, `vert` becomes Y and the plan's Y becomes Z, so the floor lies in the XZ plane, and seen from above, +Z points down on the screen. Because the plan's Y grows downwards, a positive `rotate.vert` on the plan, which turns clockwise on the screen, is a negative rotation around the Y axis of model space. The same holds for the other axes: every [rotation on the plan](#rotate) turns clockwise, so a positive rotation on the plan is a negative rotation around the matching axis of model space.

Matching the scale of the plan, given in centimeters, with the scale of the [external models](#external-models) is the viewer's responsibility. This specification recommends that every model, both the one built from the plan and the imported ones, is expressed in meters.

## Nodes

A node can have a list of children. It may also refer to a room, apply a list of [transforms](#transforms), or both.

- **Names.** A node may have a `name`, unique in the project. Other nodes refer to it by this name, for example in [wiring](#wiring-objects-together). The name is optional, but a node without one cannot be wired to. The names of [openings](#openings-doors-and-windows) share this namespace, as every opening with a model is filled by a node of its own. Names of nodes and openings must not contain a slash (`/`), which separates the node from the object in a wiring.
- **Top-level nodes.** A top-level node names the `room` it places its children in. For convenience, the origin of a room is the top-left corner of the bounding rectangle of the room's floor. The walls around the floor do not count, so a wall's width does not change where the origin lies. The only rotation allowed at this level is `rotate.vert` by a multiple of 90°. A `rotate` with an `x` or a `y` component, or with a `vert` component that is not a multiple of 90°, is an error. Such a rotation moves the origin to another corner of that rectangle, with no translation needed: 90° moves it to the top-right corner, 180° to the bottom-right and 270° to the bottom-left. Negative scaling, used to place child nodes from right to left, does need an explicit translation. Without it, the models appear past the left end of the locally top wall, not along it.
- **Choosing a wall.** More than one top-level node may refer to the same room, which makes it easy to pick a wall: each rotation makes a different wall the locally top one, closest to (0, 0) and running along the new X axis.
- **Lower-level nodes.** Nodes below the top level cannot refer to rooms, but their rotations are not limited to multiples of 90°. A node's transforms are applied on top of its parent's matrix, and the resulting matrix places the node's `model`.

```yaml
nodes:
- room: bathroom
  transform:
    - rotate.vert: 90 # the top-right corner is the origin
  children:
    - name: bathroom sink
      model: bathroom sink
      transform:
        # after the rotation, X runs down the right wall:
        # move the sink 100 centimeters along it from
        # the corner and raise it by 30 centimeters
        - translate.x: 100
        - translate.vert: 30
```

### Node properties

| Property | Required | Default | Meaning |
| -------- | -------- | ------- | ------- |
| `name` | no | | Name of the node, unique among nodes and openings, without a `/`. |
| `room` | top level only | | `name` of the [room](#rooms) the node places its children in. Not allowed below the top level. |
| `transform` | no | `[]` | List of [transforms](#transforms). |
| `children` | no | `[]` | List of child nodes. |
| `model` | below the top level only | | Name of the [external model](#external-models) the node places. Not allowed at the top level. |
| `state` | below the top level only | | Starting state of the model, as described in [External models](#external-models). |
| `wires` | below the top level only | | [Wiring](#wiring-objects-together) of the model's output slots. |

## Transforms

For more advanced placement, a node can have a list of transform operations. The list is a sequence: each operation takes the matrix from the step before it, applies its own transformation and hands the result to the next one. The first operation starts from the parent's matrix. The arguments use these units:

| Operation | Unit |
| --------- | ---- |
| translate | centimeters |
| rotate | degrees |
| scale | plain numbers (factors) |

### Translate

`translate` takes one of the forms below.

```yaml
transform:
  # either `pos` or `vert` may be missing
  - translate: { pos: [200, 300], vert: 90 }
  - translate: [100, 200] # x and y only (movement on the plan)
  - translate.x: 200
  - translate.y: 300
  - translate.vert: 90 # up from the floor
```

### Rotate

`rotate` takes one of the forms below. Every rotation on the plan turns clockwise for positive angles, looking from the positive end of its axis toward the origin. For `vert`, this means clockwise on the screen, like the [direction of a wall](#placement-on-the-plan).

```yaml
transform:
  # any of `x`, `y` or `vert` may be missing
  - rotate: { x: 20, y: 30, vert: 90 }
  - rotate.x: 20 # around the plan's X axis (horizontal on the screen)
  - rotate.y: 30 # around the plan's Y axis (vertical on the screen)
  - rotate.vert: 90 # around the vertical axis, perpendicular to the plan
```

### Scale

`scale` takes one of the forms below. Each factor multiplies the size along one axis: 1 keeps it, 2 doubles it, 0.5 halves it and -1 mirrors it.

```yaml
transform:
  - scale: 2 # the same factor along every axis
  # any of `x`, `y` or `vert` may be missing; a missing factor is 1
  - scale: { x: 2, y: 0.5, vert: 1.5 }
  - scale: [2, 0.5] # x and y only (on the plan); the vertical size is kept
  - scale.x: 2 # along the plan's X axis (horizontal on the screen)
  - scale.y: 0.5 # along the plan's Y axis (vertical on the screen)
  - scale.vert: 1.5 # along the vertical axis, perpendicular to the plan
```

## Walls and rooms

A project describes the floor plan as a series of walls, which divide the plan into rooms.

- **Walls.** A wall is, in essence, a line on the plan from point A to point B, widened on both sides to a band of constant width.
- **Rooms.** A room is defined by the walls it touches and the walls it avoids. It is an error for two rooms to select the same area.
- **Invisible walls.** Some open spaces span several rooms, for example a living room, kitchen annex and hallway. To divide them, use invisible walls. They exist on the plan and take part in splitting it into separate rooms, but are removed afterwards. An invisible wall is declared like any other wall, with `invisible: true`, but works as a knife of zero width: it has no width, height or elevation, no faces, and it cannot have openings, because it _is_ an opening. Giving an invisible wall any of `width`, `height`, `elevation` or `openings` is an error. Only its `name` and its placement remain: `origin`, `direction`, `length` and `measurement`.
- **Overlapping walls.** Walls may overlap on the plan, including lower and elevated walls placed one above the other.
  - When the floors of the rooms are cut out, the overlapping walls are laid on top of each other, each as the rectangle it projects onto the plan. An overlap never creates a room of its own.
  - When the model is built, walls on the same line are merged into one wall made of bands. Each part keeps its own height and elevation, and the heights covered by none of them stay empty. For example, with a 262 cm ceiling, three walls on the same line — 50 cm tall on the floor, 50 cm tall at 100 cm elevation and 100 cm tall at 200 cm elevation — make one wall that is, from the floor up: 50 cm of wall, 50 cm empty, 50 cm of wall, 50 cm empty and 62 cm of wall up to the ceiling; the remaining 38 cm of the top wall are capped by the ceiling.
  - Parts that also overlap in height join into a single band. In the example above, a fourth wall, 50 cm tall at 125 cm elevation, would extend the middle band to 75 cm (from 100 cm to 175 cm) and shrink the upper empty space to 25 cm.
  - With overlaps of varying lengths, heights, elevations, starting points and directions, the resulting solid may be quite irregular.

### Walls

#### Placement on the plan

A wall is placed by its `origin`, its `direction` (an angle in degrees) and its `length`. All three are required. The point given in `origin` depends on `measurement`, described below; it is not always the start of the wall's line.

Walls are placed only after the whole project is read, so the order in which they are declared, within a file or across files, does not change where they are placed. It only breaks a tie between [overlapping strips](#plinths-and-moldings) of equal size. Walls that start from other walls must not form a cycle. A viewer must detect such a cycle and report it as an error.

- **Direction.** An angle of 0 points along the X axis (right on the screen), and 90 along the Y axis (down on the screen). As Y grows downwards, positive angles turn clockwise; -90 points up.
- **Left and right.** The sides of a wall are named as if looking along the wall, from its start toward its end. This is like the banks of a river, which are named looking downstream, from the source toward the mouth.
- **The line and the measured length.** The line from A to B runs through the middle of the wall. A and B are the midpoints of the wall's two ends: point A is the calculated start of the wall, and point B its calculated end. The person drawing the plan often cannot reach A and B, or measure the distance between them. So `measurement` says where the length was measured, and with it, which point `origin` gives:
  - `inner`, the default, measures along the line from A to B. `origin` is point A.
  - `left` and `right` measure along the left or the right face of the wall, between its **visible corners**, described in the next point. `origin` is the visible corner at the start of the measured face. Points A and B are then found from it, as described in **From the measurement to A and B** below.

  Elsewhere in this document, the start of a wall always means point A, never the point given in `origin`.
- **Visible corners.** Walls are joined by their lines, which have no width, but the walls themselves do. Near a joint, the end of a face is buried inside the neighboring wall, and the person measuring can only reach the corner where the face comes out of it. So a corner on a face is found like this: start at the corner of the wall's band, then move along the face, away from that end, to the first point that lies outside every other wall. If the band's corner is not inside another wall, it is the visible corner.
- **Finding the starting point for measurements.** An `offset` along a wall is always measured from the visible start of one of that wall's sides. For an opening, the _side_ is its `measured-side` property. For a wall starting from another wall, it is the `side` given in `origin`, or the side inferred as described in [Inferring direction and side](#inferring-direction-and-side).
  1. Take point A of the wall the measurement is taken along.
  2. Move it across the wall to the indicated _side_.
  3. If the point lies inside another wall, move along the edge of that side until it comes out, at the visible corner. The point reached after this step is the reference point for the measurement.
  4. Move along the edge of that side by `offset` centimeters.

  The resulting point may lie beyond either end of the measured wall; this is allowed.
- **Starting from another wall.** Instead of coordinates, the start of the wall can be given relative to another wall, whose `name` is given in `from`. The starting point on the referenced wall is found as described in the previous point, with the `offset` given in `origin`, which defaults to 0.

  For most angles, the starting point lies on the face of the referenced wall and takes the place of the coordinates in `origin`. Point A is found from it as described below. Keep in mind that `left` and `right` measure only the visible part of the new wall, while with `inner`, point A lies further back, on the midline of the referenced wall, so the declared `length` must include that part as well.

  For parallel walls, whose directions differ by 0° or 180°, the starting point _does not_ stay on the face of the referenced wall. An additional step moves it back across the referenced wall to its midline, which makes the new wall collinear with the referenced one. Such a wall always uses `inner` measurement, and giving `measurement` explicitly is an error.
- **From the measurement to A and B.** This step is needed for `left` and `right` measurements, and for every wall that starts from another wall. With `inner` measurement and coordinates in `origin`, the given point already is A. Take the cross-section of the wall at the start of the measurement, across the wall's direction, and find its midpoint.
  - If the wall touches no other wall there, the midpoint is point A.
  - If it touches another wall, move the midpoint back along the wall's line, into the other wall, until both faces of the new wall have reached it. That point is A.
  - For a wall that starts from another wall, move the midpoint back until it reaches the midline of the referenced wall. That point is A.

  Point B is found the same way at the far end of the measurement, if another wall meets the wall there. The wall's length is then the distance from A to B.

> [!NOTE]
> A wall that starts from another, thinner wall is not allowed to poke out on the other side of it. For instance, if a thicker wall starts from the right side of a thinner wall at an angle of 15°, its projected rectangle alone would leave an extra triangle on the left of the thinner wall. This piece is detected and removed early in the calculations.

#### Inferring direction and side

For a wall that starts from another wall, both its direction and the side of the referenced wall may need to be calculated. The side can only be inferred once the direction is known:

1. Take the `direction` of the new wall. If it is a number, it is the angle in degrees described under [Placement on the plan](#placement-on-the-plan) and the algorithm ends.
2. If the `origin` is an object with a `from` property, use it to locate the referenced wall. Otherwise, it is an error.
3. Take the calculated direction of the referenced wall.
4. If the new wall's direction is the string `left`, subtract a right angle; if it is `right`, add a right angle.

If the `side` of the referenced wall is not given, infer it from the two directions by normalizing `new.direction - referenced.direction` to the range (-180°, 180°]. A negative result means `left`, otherwise `right`. A difference of 0° or 180° lays the new wall on the same line as the referenced one (see **Starting from another wall** in [Placement on the plan](#placement-on-the-plan)), and giving an explicit `side` is an error; the parts where they overlap are merged, as described under **Overlapping walls** in [Walls and rooms](#walls-and-rooms).

#### Height, width and elevation

- **Defaults.** A wall's elevation is 0. Its height and width come from the project defaults. Each wall can override any of the three.
- **Ceiling.** The project's default wall height is also the height of the ceiling.
- **Taller walls.** A wall exceeding the ceiling is capped at ceiling height.
- **Walls above the ceiling.** A wall whose `elevation` is at or above the ceiling height has nothing left below the ceiling, so it is not built into the model. It still splits the plan into rooms, like an [invisible wall](#walls-and-rooms), but with its width: the floor and the ceiling across that width belong to the rooms as described below for elevated walls.
- **Lower and elevated walls.** A wall that is lower than the ceiling, or raised above the floor, exposes new faces of two kinds:
  - _Floor and ceiling faces_: the floor below an elevated wall, at elevation 0 and facing upwards, and the ceiling above a lower wall, at the height of the ceiling and facing downwards. These faces logically become part of the floor or the ceiling.
  - _Wall faces_: the top of a lower wall, the bottom of an elevated wall, and the faces where it meets neighboring walls.

  Like every other face of the wall, an exposed face is assigned to a room, and takes its material as described in [Rooms → Materials](#materials). A face that lies across the wall's line, such as the top of a lower wall or the floor below an elevated one, is split along that line when the wall stands between two rooms, and each part goes to the room on its side. A face that looks into one room only, such as the end of a wall, goes to that room.

#### Openings, doors and windows

Openings belong to a wall, but they do not take part in splitting the floor plan into rooms. In the final model, each opening is cut out of the wall's mesh as a rectangular hole. When the wall belongs to a single room, for example an outer wall with a window, all the new faces of the hole belong to that room. Otherwise, a threshold is found and the new faces of the hole are assigned to the neighboring rooms:

- If the opening has no model, the threshold runs through the middle of the opening and each room gets half of the new faces.
- If the opening is on the `right` side of the wall, which is the default, the room on the left gets all of the new faces.
- If it is on the `left` side, the room on the right gets them.

The new faces take their materials as described in [Rooms → Materials](#materials). The bottom of a door hole, at elevation 0, is a floor face. Every other new face, the sill of a window and the top of the hole included, is a wall face of this wall.

An opening starts `offset` centimeters from the reference point on its `measured-side`, measured to the edge of the opening closer to the start of the wall. For details, see **Finding the starting point for measurements** in [Placement on the plan](#placement-on-the-plan). The `offset` is required. The `measured-side` may be omitted only if every wall the start of this wall is embedded in is either perpendicular to it or collinear with it, because then both sides give the same reference point; `right` is used in that case. An opening must also declare its `size` as `[<width>, <height>]`, both in centimeters. A window or any other opening that does not reach the floor sets its `elevation`, the height of its bottom edge above the floor in centimeters; it defaults to 0. It may name a [model](#external-models) that fills it with `model`. The model's geometry is not scaled, so it is up to the author to pick a model that fits the hole. An opening may also have a `name`, unique in the project and shared with the names of the [nodes](#nodes), so that other nodes can wire to its model.

Some openings are unusual. Any of them may be what the author intended, or a mistake; the format does not decide which, and treats them all the same way, following the rules above:

- Openings that overlap in the same wall: each hole is cut, so together they leave one larger hole.
- An opening that crosses another wall: a hole is cut only in the wall that declares the opening, so the other wall stays whole.
- An opening that lies partly outside its wall, below it, above it or past either of its ends: the hole is cut only where the opening and the wall overlap, the same way the bands of [overlapping walls](#walls-and-rooms) are combined. For example, an opening that reaches past the end of its wall leaves only the part of the wall above the opening, which gives the same result as an elevated wall. If the opening is also elevated, the strip of wall below it, between the floor and the opening's bottom edge, is left as well.
- An opening without a `model` that lies wholly outside its wall: it is quietly ignored.
- An opening with a `model` that lies partly or wholly outside its wall: the model is still placed, as for any other opening.

> [!NOTE]
> An opening without a `model`, with no `elevation`, as tall as the ceiling and spanning at least the whole length of its wall, removes the wall from the model, while the wall still splits the plan into rooms. This is a roundabout way of making an [invisible wall](#walls-and-rooms) and is discouraged.

An opening with a model is filled by a node synthesized for it. The node takes the `name`, `state`, `model` and `wires` of the opening, and is placed at the model's origin in the opening. All synthesized nodes are children of a room that exists in the project but has no name, so it cannot be referred to: no node can be placed in it, and its only children are the synthesized nodes. Its origin is the origin of the plan, so the synthesized nodes are placed in the plan's coordinates.

The position of a synthesized node follows these rules:

- Every model is assumed to be built around the point on the floor below the middle of the opening, on the line of the wall, halfway through opening's width. For an elevated opening, the node is raised by the opening's `elevation`.
- Every model is also assumed to be designed for the right side of the wall, which is the default `side` of an opening, and to open to the left. If an opening says otherwise, the node's transform is first rotated 180° around the vertical axis (for `side: left` of the wall) and then mirrored with `scale.x: -1` (for `opens: to-right`).

> [!NOTE]
> Door and window models bridge the plan and the [model space](#model-space). They are written in model space, but laid out for the plan: the opening lies in the XY plane, centered on the Y axis and standing on Y = 0, while Z runs through the thickness of the wall. A door frame, for example, has its posts at -X and +X and its head at +Y. The front of the model, +Z, faces the right side of the wall. Seen from that side, +X lies to the right of the opening's axis, so the model's X runs along the direction of the wall. Once the plan is converted to model space, the model fits its opening as it is, only raised by the opening's `elevation`.

As the model is placed by a node, it starts in the state chosen as described in [External models](#external-models), and the state can later be changed through [interactions](#interactions).

In openings without a `model`, the `opens`, `side`, `state` and `wires` properties are ignored. Such an opening synthesizes no node, but its `name` is still reserved: it must not clash with the name of another opening or node.

#### Opening properties

| Property | Required | Default | Meaning |
| -------- | -------- | ------- | ------- |
| `name` | no | | Name of the opening, unique among nodes and openings, without a `/`. |
| `offset` | yes | | Distance from the reference point to the edge of the opening, in centimeters. |
| `measured-side` | see above | `right` | Side of the wall the `offset` is measured on: `left` or `right`. |
| `size` | yes | | `[<width>, <height>]` of the hole, in centimeters. |
| `elevation` | no | 0 | Height of the bottom edge above the floor, in centimeters. |
| `model` | no | | Name of the [external model](#external-models) that fills the opening. |
| `side` | no | `right` | Side of the wall the model is designed for: `left` or `right`. |
| `opens` | no | `to-left` | Direction the model opens to: `to-left` or `to-right`. |
| `state` | no | | Starting state of the model, as described in [External models](#external-models). |
| `wires` | no | | [Wiring](#wiring-objects-together) of the model's output slots, as in a node. |

#### Wall properties

| Property | Required | Default | Meaning |
| -------- | -------- | ------- | ------- |
| `name` | no | | Name of the wall, unique among walls. Other walls refer to it in `from`, and rooms in `touches`, `avoids` and `faces`. A wall without a name cannot be referred to. |
| `origin` | yes | | `[<x>, <y>]` on the plan, or an object with `from`, `side` and `offset`; see [Placement on the plan](#placement-on-the-plan). |
| `direction` | yes | | Angle in degrees, or `left` or `right` for a wall starting from another wall. |
| `length` | yes | | Length in centimeters, measured as `measurement` says. |
| `measurement` | no | `inner` | `inner`, `left` or `right`. |
| `width` | no | `wall-width` default | Width in centimeters. Not allowed on an invisible wall. |
| `height` | no | `wall-height` default | Height in centimeters. Not allowed on an invisible wall. |
| `elevation` | no | 0 | Height of the bottom of the wall above the floor, in centimeters. Not allowed on an invisible wall. |
| `invisible` | no | `false` | Makes the wall [invisible](#walls-and-rooms). |
| `openings` | no | `[]` | List of [openings](#openings-doors-and-windows). Not allowed on an invisible wall. |

#### Wall example

```yaml
walls:
- name: W123
  direction: 90
  origin: [180.2, 0]
  length: 368.3
  width: 18 # load-bearing wall

- direction: 180 # could also be "right" (90° + 90°), as `origin` references another wall
  origin:
    from: W123
    side: right # can be inferred from both directions
    offset: 30
  length: 400
  measurement: right # other possible values: left or inner
  openings:
    - name: bathroom door
      offset: 60
      size: [92, 207]
      model: bathroom
      opens: to-left
      side: right # the default, may be omitted
      state: closed
    - name: bathroom window
      offset: 220
      # this wall is perpendicular to W123, so "left"
      # lands in the same place as the default "right"
      measured-side: left
      size: [115, 215]
      elevation: 15
      opens: to-left
      model: top window
      state: opened
models:
  bathroom: models/bathroom-door.glb
  top window: models/top-window.glb
```

## Rooms

A room is a part of the plan enclosed by walls, as described in [Walls and rooms](#walls-and-rooms). The `rooms` section names the rooms, so that [nodes](#nodes) can refer to them, and sets the look of their faces: materials, plinths and moldings.

- **Identity.** A room may have a `name`, unique among rooms. Top-level nodes and cameras refer to a room by this `name`. The `name` is optional, but nothing can be placed in a room without one, so a viewer may warn about it.
- **Area.** `touches` lists the walls the room touches, and `avoids` the walls it must not reach. Together they select the areas, among those the walls divide the plan into, that the room covers. An area is selected when every wall in `touches` is among the walls bounding the area, and no wall in `avoids` is. In other words, the intersection of `touches` with the bounding walls equals `touches`, and the intersection of `avoids` with the bounding walls is empty.
  - If they select no area, a viewer may warn about it.
  - If they select several areas, the room covers all of them as one room. This holds whether the areas are separated only by a shared wall, with or without openings, or by other areas.
  - An area selected by no room gets the project defaults: its materials, plinth and molding.
- **Outside.** The rest of the plan, around the outer walls, is not an area: no room can select it. Version 1 assigns no materials to it, nor to the faces of the outer walls that look at it. Whether a viewer draws these faces with a material of its choice, leaves them see-through, or lets the user switch between the two, is outside the scope of this document.
- **Unclosed walls.** The outer walls must close the plan. A gap between two walls, unless another wall, visible or [invisible](#walls-and-rooms), closes it, makes the region between unclosed walls part of the outside, and no room can select it. This is an easy mistake to make, for example with a wall a little too short to reach the next one. A room that selects no area because of it gives a [warning](#warnings).

### Materials

A room may set three materials in its `materials` map: `floor`, `walls` and `ceiling`, each of them optional. Each value is the name of a material described in [Room materials](#room-materials).

```yaml
rooms:
  - name: bathroom
    touches: [W123, W124, W130]
    materials:
      floor: gray tiles
      walls: white tiles
      # no ceiling, so the default materials.ceiling is used
```

Materials are chosen once the walls are cut and every resulting face is assigned to a room, as described for [lower and elevated walls](#height-width-and-elevation) and for [openings](#openings-doors-and-windows). Every face is then one of three kinds, decided by its place, not by its direction alone:

- **Floor:** a horizontal face at elevation 0, facing upwards.
- **Ceiling:** a horizontal face at the height of the ceiling, facing downwards.
- **Wall:** every other face, whatever its direction, for example the top of a lower wall, the bottom of an elevated wall or the sill of a window. A wall face comes from one wall.

A floor or ceiling face uses the first of these that is set:

1. The `floor` or `ceiling` material of the room.
2. The `floor` or `ceiling` of the `materials` in `defaults`.
3. A deliberately distinct material, such as a magenta texture, so that the gap is easy to spot.

A wall face uses the first of these that is set:

1. The override for its wall in the room's `faces`, as described in [Overriding single faces](#overriding-single-faces).
2. The `walls` material of the room.
3. The `walls` of the `materials` in `defaults`.
4. The deliberately distinct material.

The faces of an area selected by no room skip the steps that refer to the room, and so take the project defaults. Where [walls merged on one line](#walls-and-rooms) make up a single face, the face is first split wherever the set of walls covering it changes, so that each part is covered either by one wall or by the same overlapping walls along its whole extent. A part covered by one wall comes from that wall. A part where several walls overlap counts as coming from the wall that is first in the [combined](#general-notes) `walls` section.

Up to this point, every wall face belongs to one room and one wall, and every floor or ceiling face to one room. Once the materials are selected, touching coplanar faces with the same material, that is faces in one plane that share an edge of positive length, are merged into one face before the model is handed over to the viewer. A merged face may span several walls or rooms, so the viewer cannot tell which room or wall it belongs to. Everything that depends on that, such as [face overrides](#overriding-single-faces), [plinths and moldings](#plinths-and-moldings) and [tile patterns](#tiles-across-neighboring-faces), is decided on the faces from before the merge.

### Plinths and moldings

A plinth runs along the bottom of the room's walls, on the floor. A molding runs along their top, under the ceiling. Both are set with the same properties:

- `profile` is the plane figure that forms the strip when it is swept along the face of the wall, the way a figure rotated around an axis forms a solid of revolution. It is either `[<depth>, <height>]` in centimeters, for a plain rectangular strip, or a list of points outlining the figure. Each point is a pair of distances in centimeters: first from the wall, then from the surface the strip rests on, which is the floor for a plinth and the ceiling for a molding. A profile used for a molding therefore comes out upside down compared to the same profile used for a plinth. The outline starts on that surface, ends on the wall, and is closed through the corner where they meet. If the first point is not on the surface, an edge straight from the surface to it is added. If the last point is not on the wall, an edge from it straight to the wall is added.
- `material` is the material of the strip.

A room takes its `plinth` and `molding` from the project defaults, unless it sets its own. A room's own `plinth` or `molding` replaces the default as a whole. The value `none` is valid too: it removes a default the room would otherwise get, for example the plinth in a tiled bathroom. Without a plinth or a molding in the room and in the defaults, the room has none, which is not an error.

A plinth or molding without a `material` gets the deliberately distinct material, as faces do.

Plinths and moldings follow the faces of the room:

- **Reach.** A plinth runs only along the parts of the faces that reach the floor, and a molding only along the parts that reach the ceiling. The face under an elevated wall gets no plinth, and the face above a lower wall gets no molding.
- **Corners.** Where two faces of the room meet, the strips are mitred.
- **Openings.** A plinth stops at the sides of an opening that reaches the floor, such as a door. A molding stops at an opening that reaches the ceiling.
- **Different strips in a row.** Where the strip changes along one face, for example where the face of one wall ends and the face of a collinear wall with another override begins, each strip runs up to that point and is cut square. The end of the deeper strip is left exposed and closed by a flat cap, as a carpenter would do.
- **Overlapping strips.** Where two strips occupy the same space, for example where [merged walls](#walls-and-rooms) overlap along their length, or where two faces meet at an angle so sharp that the mitred strips run into each other, they are merged into one solid. Over the length they share, the cross-section is the union of the two profiles. If the strips have different materials, the merged part takes the material of the strip whose profile has the larger area; with equal areas, the strip of the wall that comes first in the [combined](#general-notes) `walls` section wins.

### Overriding single faces

A room may change the material, plinth or molding of single walls in `faces`, a map from a wall's `name` to an override. The override applies to every wall face of that wall that belongs to the room: both sides of a wall standing inside the room, its top, bottom and ends, the step faces described below, and the faces of the holes cut for its openings. Floor and ceiling faces belong to no wall, so they are never overridden. A face override takes `material`, `plinth` and `molding`, with the same values and the same `none` as above.

Where two parallel walls meet and their faces looking into the room do not lie in one plane, a short step face joins them. The step is the uncovered part of the end of the wall that sticks out further into the room, so it is a face of that wall: it takes the wall's material, plinth and molding, including any override set for that wall in `faces`.

### Room properties

| Property | Required | Default | Meaning |
| -------- | -------- | ------- | ------- |
| `name` | no | | Name of the room, unique among rooms. |
| `touches` | yes | | Names of the walls the room touches. |
| `avoids` | no | `[]` | Names of the walls the room must not reach. |
| `materials` | no | | Map with the `floor`, `walls` and `ceiling` materials; each falls back on the matching key of the default `materials`. |
| `plinth` | no | `plinth` default | `profile` and `material` of the plinth, or `none`. |
| `molding` | no | `molding` default | `profile` and `material` of the molding, or `none`. |
| `faces` | no | | Overrides of the wall faces of single walls, by wall `name`. |

### Room example

```yaml
defaults:
  plinth:
    profile: [1.5, 8]
    material: white paint

rooms:
  - name: bathroom
    touches: [W123, W124, W130]
    materials:
      floor: gray tiles
      walls: white tiles
    plinth: none # tiles reach the floor
  - name: living room
    touches: [W124, W125, W126]
    avoids: [W130]
    materials:
      walls: mint paint
    molding:
      # 6 cm out from the wall, down to the wall
      # 6 cm below the ceiling. The first point is
      # 1 cm below the ceiling, not on it, so an
      # edge from [6, 0] is added in front of it.
      profile:
        - [6, 1]
        - [4, 2]
        - [2.5, 4]
        - [1, 5]
        - [0, 6]
      material: white paint
    faces:
      W125:
        material: red brick
        plinth: none
```

## Room materials

The top-level `materials` section is a list of the materials that the faces of the house, plinths and moldings name: in the `materials` of `defaults` and of a room, and in the `material` of a plinth, a molding or a [face override](#overriding-single-faces). Every material has:

- `name`, unique in the project, by which the faces refer to it, for example `white paint` or `gray tiles`,
- `type`, either `paint` or `tiles`, which decides the rest of the properties.

A face that names a material missing from the list gets the deliberately distinct material, as described in [Materials](#materials), so that the mistake is easy to spot. The same happens to a tiles material whose `texture` is missing or cannot be read. A missing or unreadable `bumps` picture leaves the tile flat.

Like `include`, every path in a material is resolved relative to the directory of the file that declares the material.

### Paint

A paint covers the face with one even color:

- `color` is the color in sRGB, written as `"#rrggbb"` or as `[<r>, <g>, <b>]` with each value from 0 to 255.
- `alpha` _(optional)_ a floating-point number in [0, 1] range to create semi-transparent faces. It defaults to fully opaque, or 1.
- `roughness` _(optional)_ runs from 0 for a mirror-like gloss to 1 for a fully matte surface. It defaults to 1.
- `metallic` _(optional)_ runs from 0 for paint, plaster or wood to 1 for bare metal. It defaults to 0.

### Tiles

Tiles cover the face with a grid of identical tiles separated by grout, the way ceramic tiles, bricks or floor panels are laid:

- `size` is the size of a single tile, as `[<width>, <height>]` in centimeters, without the grout.
- `texture` is the path to the picture of a single tile. The picture is stretched to `size`, so every tile on the face looks the same.
- `bumps` _(optional)_ is the path to a grayscale picture of the same tile, describing its relief: white is the highest point and black the lowest. Without it, the tile is flat.
- `roughness` and `metallic` _(optional)_ apply to the whole tile, with the same meaning and defaults as for a [paint](#paint).
- `offset.x` or `offset.y` _(optional)_, but not both, staggers the tiles, in centimeters:
  - Without either of them, the tiles form a plain grid.
  - With `offset.x`, every row is shifted along the row by that distance from the row above it, as in a brick wall.
  - With `offset.y`, every column is shifted along the column by that distance from the column to its left.
- `grout` _(optional)_ describes the gaps between the tiles. Without it, the tiles are laid edge to edge, as with a grout narrower than 1 millimeter:
  - `width` is the width of the gap, in centimeters. If the `width` is below 1 millimeter, the whole grout is treated as if it was not used in this tiling.
  - `color` is the color of the grout, written as for a paint.
  - `depth` _(optional)_ is how far the grout is sunk below the tiles, in centimeters. It defaults to 0.2. The relief of the tiles and the sunk grout together give the bump map of the face.

The pattern repeats along the whole face, every `width` plus the grout `width` across and every `height` plus the grout `width` up. On a wall face, rows run horizontally and the lowest row rests on the floor. On a floor or a ceiling, rows run along the plan's X axis and the grid starts at the plan's origin.

#### Tiles across neighboring faces

Where two faces next to each other use the same tiles, the pattern continues from one face to the other across their shared edge, as if the tiles were laid in one go. In a corner of a room, the row on one wall carries on around the corner to the next wall.

Within a group of such faces, the pattern starts on one face and flows from face to face over the shared edges. If the group holds both floor and wall faces, the pattern starts on the floor and spills over to the walls.

For now, the starting face is always chosen automatically, as described below. A later version of the format may let the project name it.

In a group of floor or ceiling faces, the starting face is found like this:

1. Look at the faces as they lie on the plan. Ceiling faces are projected onto it, ignoring their elevation.
2. Take the smallest rectangle that holds all the faces of the group. Its top-left corner, with the smallest X and Y, is the point O.
3. For each face, find its vertex closest to O, and sort the faces by the distance of that vertex from O.
4. Take the faces at the smallest distance. If there is only one, it is the starting face.
5. Otherwise, sweep clockwise around O, starting from the direction of the X axis, and take the face whose closest vertex is met first.
6. If several faces share that vertex, sweep again over their centroids.

In a group of wall faces, the starting face is found like this:

1. Look at the faces on the plan, as for floors.
2. Leave out the faces exposed by [lower and elevated walls](#height-width-and-elevation), such as the tops, the bottoms and the ends of the walls. The remaining faces of the walls are the candidates.
3. Reduce each candidate to the line it projects to on the floor.
4. Take the smallest rectangle that holds all the lines. Its top-left corner, with the smallest X and Y, is the point O.
5. For each line, find its end closest to O, and continue with steps 3 to 5 for floors, with the lines in place of the faces.
6. If several lines share that end, for example two walls meeting in a corner, sweep again over their other ends.
7. If the lines share the other end as well, take the face whose normal points against the sweep. A point just off that face, along its normal, is met by the sweep before the face itself.
8. If several such faces remain, for example the bands of lower and elevated walls [merged](#walls-and-rooms) one above another, take the lowest one.

> [!NOTE]
> Where on the starting wall face the pattern starts, and how it is oriented there, is not decided yet.

A face reachable in more than one way, for example the top of a low wall that boxes in a concealed toilet cistern, takes the pattern from one neighbor only, chosen in this order:

1. The neighbor on the shortest path from the starting face, counted in shared edges.
2. Among equally short paths, the neighbor that is closer to vertical.
3. If that still leaves a tie, the first neighbor going clockwise around the face, as seen on the plan.

### Material example

```yaml
materials:
  - name: mint paint
    type: paint
    color: "#b5e3c8"
  - name: white paint
    type: paint
    color: [250, 250, 248]
    roughness: 0.6 # satin finish, for plinths and moldings
  - name: white tiles
    type: tiles
    size: [30, 60]
    texture: tiles/white-glazed.png
    roughness: 0.2
    grout:
      width: 0.3
      color: "#d8d8d8"
  - name: red brick
    type: tiles
    size: [25, 6.5]
    texture: tiles/brick.png
    bumps: tiles/brick-bumps.png
    offset.x: 12.5 # half a brick
    grout:
      width: 1
      color: [190, 185, 175]
      depth: 0.5
```

## Interactions

> [!IMPORTANT]
> A new draft for the model extension will be available elsewhere soon.

As far as this document is concerned, every model exposes a list of active objects: a single `door`, for example, or the `left switch`, `middle switch` and `bottom switch` of a triple switch.

An object may declare input and output **slots**. Each slot has a name, a type and a direction, and its value can change while the house is viewed. Two objects placed by different nodes are wired together by connecting chosen slots, so that one object can update the other. When the two objects share several slots with the same name and type and with opposite directions, all of them can be connected at once.

The name of an active object must be unique in its model, and must also differ from the names of the model's lights. If an object and a light share a name anyway, a wiring that names them reaches the light. Names of active objects, lights and slots must not contain a dot (`.`) or a slash (`/`), as these characters separate the parts of a [wiring](#wiring-objects-together).

Lights are a special case. A light declares no slots of its own, but every light has the same two input slots:

- `enabled`, a boolean, which turns the light on and off,
- `intensity`, a number clamped to the range from 0 to 1, inclusive.

The light shines with the intensity declared in its model, multiplied by `intensity`, or not at all if `enabled` is false.

### Wiring objects together

When two nodes in the node tree both place a model, one of them can wire the output slots of its active objects to the input slots of the active objects or lights of the other.

An opening with a model counts as such a node, as it is filled by a node of its own: a door or a window may have triggers, too.

The wiring is declared in the `wires` map of the node or the opening that places the model with the output slots:

- Each key names an output slot of this node's model as `"<object>.<slot>"`.
- Each value is a reference to an input slot, or a list of such references. A reference has the form `"<node>/<object>.<slot>"`.

Both sides can be shortened:

- **Automatic wiring.** Without the slot names, written as `"<object>"` and `"<node>/<object>"`, every output slot of the object is connected to the input slot of the other object that has the same name and type.
- **First object.** Without the object name, written as `".<slot>"` in a key or `"<node>/.<slot>"` in a reference, the side uses the [first active object](#first-active-object) of its model.
- **Both at once.** A key with automatic wiring and the first object is the empty string, `""`, and such a reference is just `"<node>"`.

Every node named in a wiring, on either side, must exist. A missing node is an error, as the project contradicts itself.

Objects and slots are declared by the model, not by the project. A wiring that names an object or a slot the model does not have, or that connects an output slot to an input slot of a different type, means the project and the model disagree. Such a mistake breaks the connection, not the house. The viewer leaves the connection out and shows the project, but it must tell the user that the connection will not work, for example that a switch will not turn on its light. A model the viewer could not read, described in [External models](#external-models), leaves out every wire to or from it in the same way. See [Hard warnings](#hard-warnings).

### Several outputs to one input

An input slot may be wired to several output slots. It takes the value of the output that changed last. Anything that follows from this, such as a door moving erratically or a light blinking, is the author's decision, and gives no error or warning.

How signals travel along the wires, and so in which order signals that arrive at the same moment reach an input, is up to the viewer. A viewer is advised to propagate signals in fixed ticks, for example 16 per second, with every hop along a wire taking at least one tick. Propagating everything at once, in a single loop, is discouraged: a cycle of wires would never settle and could hang the viewer. With ticks, a cycle only makes the values oscillate, which is again the author's decision.

### First active object

Most models have one active object or light, or none at all. The shorthand described above lets a wiring refer to it without its name. It works for models with more objects too, and then refers to the first of them, found like this:

1. Take all the active objects and lights of the model, and sort them by name, comparing Unicode code points. This puts `"name10"` before `"name2"`.
2. Where a light and an active object share a name, remove the active object.
3. Where several entries still share a name, keep only the one declared first in the model.

The first entry left on the list is the first active object.

### Wiring example

```yaml
nodes:
  - room: R1
    children:
      # a triple switch, driving lights placed by the "kitchen lights",
      # "dining room" and "corridor fixture" nodes
      - model: triple switch
        wires:
          # automatic wiring of "left switch" to the first active object of
          # the model at the "kitchen lights" node
          left switch: kitchen lights
          # automatic wiring to two lights of the model
          # at the "dining room" node
          middle switch: ["dining room/table light", "dining room/other light"]
          # the full form, with object and slot names; as "bulb" is the
          # first active object, this could also be "corridor fixture/.enabled"
          right switch.enabled: corridor fixture/bulb.enabled
      - model: light switch
        wires:
          # automatic wiring of this model's first active object
          # to the first object of the model at the "corridor fixture" node,
          # here its "bulb"
          "": corridor fixture
```

## External models

The `models` section maps names to 3D model files. Each key is the name by which the project refers to the model: nodes and openings name it in `model`. Each value is the path to the model file, resolved relative to the directory of the file that declares it, like the paths in `include`.

A node or an opening that names a model missing from `models` is an error. The accepted file formats depend on the viewer. A model file that is missing, or in a format the viewer cannot read, is left out, together with every wire to or from it. Nothing is provided in its place, not even a box of the model's size, because the size is not known without reading the file. This is a [hard warning](#hard-warnings): the viewer shows the project, but must tell the user which models are missing. To do so, it may mark the place of the model in its own way, for example with an icon or a small stand-in model.

When a node places a model, the model is checked for its active objects, with their slots, variables, actions and states. Every node that places the model gets its own storage, holding the values of the slots and variables and the current state, so two copies of the same door can stand open and closed at the same time.

Each active object starts in the first of these states that exists:

1. The state named in the `state` of the node or the opening, if the object has such a state.
2. The `closed` state.
3. The first state declared for the object.

The starting values of slots and variables will be described together with the models.

```yaml
models:
  # no active objects
  block: models/block.glb

  # each with a single active object, with no slots or
  # variables, and three states: closed, ajar and opened
  door: models/inner_door.glb
  exit door: models/exit_door.glb

  # a single active object with a boolean output slot,
  # "enabled", and two states: off and on
  light switch: models/10_x_10_switch_single.glb

  # a single point light
  lamp: models/tiffany.glb

nodes:
  - room: dining room
    transform: [] # ...
    children:
      - transform: [] # ...
        children:
          # no active objects, so no state and no storage
          - name: block in dining room
            model: block
            transform: [] # ...

          - name: dining room door
            model: door
            state: ajar # the starting state
            transform: [] # ...

          - name: exit door # the name of the node
            model: exit door # the name of the model
            # no `state`, so the door starts in its "closed" state
            transform: [] # ...

          - name: dining room switch
            model: light switch
            # no `state` and no "closed" state, so the switch starts
            # in "off", its first declared state; entering a state
            # applies its settings, which here most likely set
            # the "enabled" slot to false
            wires:
              "": dining room lamp

          - name: dining room lamp
            model: lamp
```

## Cameras

Cameras are saved points of view. A viewer lists them by `name`, so that the user can jump straight to a room or a view instead of walking there. The `cameras` section is a list; the first camera on it is the one the viewer starts with.

A camera is placed on the plan like a [node](#nodes), and aimed with angles:

- `name` is shown to the viewer. It is required and unique in the project.
- `room` _(optional)_ is the `name` of a [room](#rooms). With a room, `offset` is measured from the room's origin, the top-left corner of the bounding rectangle of its floor. Without it, `offset` is measured from the origin of the plan.
- `offset` is the position of the camera on the plan, as `[<x>, <y>]` in centimeters.
- `elevation` is the height of the camera above the floor, in centimeters. It defaults to 160, roughly the eye level of a standing adult. It may be higher than the ceiling or below 0, see [Views from outside](#views-from-outside).
- `yaw` is the direction the camera looks at, in degrees on the plan, with the same convention as the [direction of a wall](#placement-on-the-plan): 0 looks along the X axis (right on the screen), and positive angles turn clockwise. It defaults to 0.
- `pitch` tilts the camera up or down, in degrees: 0 looks horizontally, positive angles look up, and negative ones down, with -90 looking straight at the floor. It defaults to 0.
- `roll` turns the camera around the direction it looks in, in degrees: positive angles turn it clockwise, as seen from behind the camera. It defaults to 0, which keeps the horizon level.
- `fovy` is the vertical field of view, in degrees. It defaults to 60. The horizontal field of view follows from the shape of the viewer's window.

### Views from outside

The floor and the ceiling of the built model have one side each, facing into the rooms, and the walls have caps on their tops and bottoms. Seen from outside the house, the floor and the ceiling are not drawn. A camera high above the ceiling looking down, or deep below the floor looking up, therefore sees the inside of every room, like a dollhouse with its roof lifted off.

### Camera example

```yaml
cameras:
  - name: Front door
    room: hallway
    offset: [40, 120]
    yaw: -90 # looking up the screen, into the hallway
  - name: Kitchen from the sofa
    room: living room
    offset: [310, 260]
    elevation: 110 # eye level of a seated person
    yaw: 200
    pitch: -10
    fovy: 70
  - name: Whole flat from above
    offset: [500, 400]
    elevation: 1200 # above the ceiling, which is not drawn from outside
    pitch: -90
    fovy: 45
  - name: Whole flat from below
    offset: [500, 400]
    elevation: -900 # below the floor, which is not drawn from outside
    pitch: 90
    fovy: 45
```

## Sky box

The sky box is the picture of the world around the house, seen through the windows and from the [views from outside](#views-from-outside). It also lights the house from outside. The `sky-box` section is optional; without it, the sky and the light around the house are up to the viewer. The section gives the picture in one of two ways:

- **An ordinary picture and a hand-written sun.** The picture, for example a PNG file, only shows the sky. The light comes from the `sun` described below.
- **An HDR picture.** The picture also carries the light. The sun is found as the brightest spot of the picture, and the rest of the picture lights the house as the ambient light.

Paths to the pictures are resolved like the paths in `include`: relative to the directory of the file that names them. If any of the pictures is missing or cannot be read, the sky box is treated as if the `sky-box` section was not given.

### Directions

The picture may show the sun, so the sky box turns with `north`, together with the [sun](#sun) light. Its sides are named after the plan's axes, as they lie with the default `north` of -90, that is with north up on the screen. Any other `north` turns the whole sky box around the vertical axis by the difference.

| Side | Direction | On the plan |
| ---- | --------- | ----------- |
| `left` | -X | left on the screen |
| `right` | +X | right on the screen |
| `back` | -Y | up on the screen |
| `front` | +Y | down on the screen, the +Z of the [model space](#model-space) |
| `down` | -vert | below the floor |
| `up` | +vert | above the ceiling |

### Pictures

An ordinary picture can be given in one of three forms:

- `cube` maps each side named above to its own picture. Each picture is turned the same way as its square in the `cross` layout below, so neighboring pictures meet along the same edges.
- `cross` is one picture with the six sides laid out as an unfolded cube. Seen from inside the cube, the middle row goes round the horizon, turning right, and the up and down sides are attached to the back side:

  ```text
        +-----+
        |up   |
  +-----+-----+-----+-----+
  |left |back |right|front|
  +-----+-----+-----+-----+
        |down |
        +-----+
  ```

- `panorama` is one picture unwrapped from a 360° photo, in the same equirectangular layout as an HDR picture. The middle of the picture looks at the back side, the left and right edges meet at the front side, and the top and bottom edges are straight up and straight down.

An HDR picture is given in `hdri` and always uses the `panorama` layout.

### Sun

With an ordinary picture, the light comes from the `sun`:

- `yaw` is the compass direction the sunlight comes from, in degrees clockwise from north: 0 is north, 90 east, 180 south and 270 west. On the plan, the sun lies in the direction `north` + `yaw`, with the plan's angles described under [Placement on the plan](#placement-on-the-plan). Instead of a number, `yaw` may be one of the 16 points of the compass rose, written with up to three letters. Only the spellings in the table are accepted, so `NE` is valid but `EN` is not:

  | Point | Yaw | Point | Yaw | Point | Yaw | Point | Yaw |
  | ----- | --- | ----- | --- | ----- | --- | ----- | --- |
  | `N` | 0 | `E` | 90 | `S` | 180 | `W` | 270 |
  | `NNE` | 22.5 | `ESE` | 112.5 | `SSW` | 202.5 | `WNW` | 292.5 |
  | `NE` | 45 | `SE` | 135 | `SW` | 225 | `NW` | 315 |
  | `ENE` | 67.5 | `SSE` | 157.5 | `WSW` | 247.5 | `NNW` | 337.5 |

  It defaults to `SW`.

  The compass points are accepted only by the sun. The `yaw` of a [camera](#cameras) is always a number on the plan.
- `pitch` is the height of the sun above the horizon, in degrees: 0 is on the horizon, and 90 straight up. It defaults to 45. A negative `pitch` puts the sun below the horizon, for dusk or night. How the house is lit then, for example with a penumbra of a sun just below the horizon, is up to the viewer.
- `intensity` is the illuminance of the direct sunlight on a surface facing the sun, in lux. It defaults to 80000.
- `ambience` is the illuminance of the ambient light, the light that comes from the whole sky and fills the shadows, on a horizontal surface, in lux. It defaults to 15000.

Together, the defaults give a clear afternoon in central Poland in late spring or summer, at about 15:00 local time.

With an HDR picture, the `sun` is optional. Any of its properties that is set overrides the value found in the picture, for example to move the sun to a different time of day without changing the picture.

### Sky box examples

```yaml
sky-box:
  cross: sky/meadow-cross.png
  sun:
    # late morning, 157.5° clockwise from north
    yaw: SSE
    pitch: 35
    intensity: 70000 # lux
    ambience: 12000 # lux
```

```yaml
sky-box:
  hdri: sky/meadow-4k.hdr
  sun:
    # lower than in the picture, for an evening light
    pitch: 15
```

## Summary of error handling

This section gathers the rules spread over the document. Where it seems to disagree with a section above, the section above wins.

### Errors

An error means the project contradicts itself. Every error can be found from the project files alone, without reading the models, textures or pictures they refer to. A project with any of these is invalid. A viewer reports the error and need not show the project.

- **The file does not match the [JSON Schema](https://raw.githubusercontent.com/mzdun/dollhouse/main/schemas/house.schema.json)**, for example a required property is missing, a value has the wrong type, or two of `cube`, `cross`, `panorama` and `hdri` are given together.
- **Project-wide requirements** that the schema cannot check, as it validates single files:
  - no file of the project gives `version`,
  - no file of the project gives `wall-width` or `wall-height` in `defaults`.
- **Includes:**
  - an included file that is missing or cannot be read,
  - a file that includes itself, directly or through other files.
- **Paths:** an absolute path or a URL, anywhere in the project.
- **Names:**
  - two nodes or openings with the same name, in one file or in different files,
  - a slash in the name of a node or an opening,
  - a dot or a slash in the name of an active object, a light or a slot.
- **Missing references:**
  - **A wall missing from `from`, `touches`, `avoids` or `faces`.** The placement of walls and the split of the plan into rooms depend on every wall. A missing wall in `avoids` is no exception: without knowing where the wall would run, no area can be said to avoid it, so the choice of every area becomes uncertain.
  - **A room missing from a top-level node or a camera.** The room sets the origin of the models and of the camera, which would otherwise move unexpectedly.
  - **A model missing from the `models` map**, named by a node or an opening.
  - **A node missing from a wiring**, on either side.
- **Walls:**
  - walls that start from one another in a cycle,
  - `direction` given as `left` or `right` without `from` in `origin`,
  - `measurement` or `side` given for a wall parallel to the wall it starts from,
  - `width`, `height`, `elevation` or `openings` on an invisible wall.
- **Rooms:** two rooms selecting the same area.
- **Openings:** `measured-side` omitted while the start of the wall is embedded in a wall that is neither perpendicular nor collinear to it.
- **Nodes:**
  - a top-level node with a rotation other than `rotate.vert` by a multiple of 90°,
  - a `room` below the top level,
  - a `model`, `state` or `wires` on a top-level node.
- **Tiles:** both `offset.x` and `offset.y` given.

### Hard warnings

A hard warning means the project and an external file disagree. It can only be found after reading the external models. The house can still be shown, with some functionality missing. A viewer shows the project, but it _must_ tell the user which functionality will not be provided.

- **A model file that is missing or in a format the viewer cannot read.** The model is left out, with every wire to or from it, and nothing is provided in its place.
- **An active object or slot missing from a wiring**, on either side, including the [first active object](#first-active-object) of a model that has none. The connection is left out.
- **A wiring that connects slots of different types.** The connection is left out, so, for example, a switch does not turn on its light.

A viewer may offer a strict mode that treats hard warnings as errors.

### Warnings

A viewer may warn about these, but shows the project anyway.

- An entry replaced by a later one with the same name, as described in [Combining files](#general-notes). A viewer should warn about this one, naming both places, for example: room "bedroom" from "rooms.yaml" is replaced by "bedroom" from "unrelated-rooms.yaml".
- A file read more than once, as described in [Includes](#general-notes). A viewer should warn about this one. A command-line tool is recommended to show the chain of includes that led to each reading, for example: "d.yaml" is read twice, through "a.yaml" → "b.yaml" → "d.yaml" and through "a.yaml" → "c.yaml" → "d.yaml".
- An external file other than a model that is missing or cannot be read. A viewer should warn about these, as they are never silent:
  - a tile `texture` gives the deliberately distinct material,
  - a `bumps` picture leaves the tile flat,
  - a sky box picture makes the sky box behave as if it was not given.
- A path whose case differs from the name of the file on disk, on a system that ignores case but preserves them. The project would not open on a system that does not.
- A room without a `name`, as nothing can be placed in it.
- A room whose `touches` and `avoids` select no area.
- Any face, plinth or molding that ends up with the deliberately distinct material, for example because it names a missing material.
- Any of the situations listed under [Leniency and silent ignores](#leniency-and-silent-ignores), when a viewer wants to point it out.

### Decisions left to the viewer

- Matching the scale of the plan, in centimeters, with the scale of the external models; meters are recommended.
- The file formats of external models.
- The look of the deliberately distinct material; a magenta texture is suggested.
- What the title bar shows without a `title`.
- Whether to show a compass for `north`.
- The sky and the light around the house without a `sky-box`.
- The light of a sun below the horizon, with a negative `pitch`.
- How signals travel along the wires; fixed ticks are advised, see [Several outputs to one input](#several-outputs-to-one-input).
- The outside of the plan and the faces of the outer walls that look at it: drawn with a material of the viewer's choice, see-through, or switchable.

### Leniency and silent ignores

These are not errors. The project is shown with the described result, without a message, unless the viewer chooses to [warn](#warnings).

- **Materials:**
  - a face that names a material missing from `materials`, or a plinth or molding without a `material`, gets the deliberately distinct material,
  - a grout narrower than 1 millimeter is treated as no grout.
- **Walls:**
  - a wall taller than the ceiling is capped at the ceiling,
  - a wall whose `elevation` is at or above the ceiling is not built, but still splits the plan into rooms.
- **Openings:**
  - overlapping openings in one wall leave one larger hole,
  - an opening that crosses another wall cuts only its own wall,
  - an opening that lies partly outside its wall is cut only where the two overlap,
  - an opening without a `model` that lies wholly outside its wall is ignored,
  - an opening with a `model` that lies partly or wholly outside its wall still places its model,
  - in an opening without a `model`, `opens`, `side`, `state` and `wires` are ignored.
- **Areas and rooms:** an area selected by no room gets the project defaults.
- **Active objects:**
  - if a node or an opening sets a `state` that one of its model's active objects does not have, that object starts in its `closed` state instead, or, if it has no `closed` state either, in the first state declared for it,
  - an active object and a light that share a name: the wiring reaches the light,
  - several entries of a model sharing a name: the [first active object](#first-active-object) is chosen from the one declared first.
