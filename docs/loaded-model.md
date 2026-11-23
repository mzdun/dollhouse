# Loaded model

A **loaded model** is a model as a viewer holds it once it has been read from a file: its nodes, meshes, materials and lights, and its active objects resolved against them. It is declared as `pyxel::gfx::loaded_model` in `pyxel-models` ([loaded_model.hpp](../libs/pyxel-models/include/pyxel/gfx/loaded_model.hpp)) and read from a file by `pyxel::io::import_model` in `pyxel-io` ([import.hpp](../libs/pyxel-io/include/pyxel/io/import.hpp)).

The loaded model is shared by every placement of the model in a scene and never changes after loading. Everything a placement changes, such as the state of its active objects, lives in the storage of that placement (see [One storage per placement](model-interactions.md#one-storage-per-placement)).

## Shape

The model is a tree of nodes kept in flat lists, which refer to each other by index, the way Assimp keeps its scenes:

```cpp
struct loaded_model {
    std::vector<node> nodes;
    std::vector<mesh> meshes;
    std::vector<material> materials;
    std::vector<light> lights;
    std::vector<active_object> objects;
};
```

### Nodes

- `nodes[0]` is the root. Every other node has the index of its `parent`.
- The nodes are in depth-first order: every parent comes before its children, and the children of a node come in their order in the file. A single pass from the front of the list therefore visits every parent before its children.
- Each node has a `name`, a `local` matrix relative to its parent, and two lists of indices: the `meshes` it draws and the `lights` it holds. Several nodes can draw the same mesh.
- `find_node(name)` returns the first node of the name, in the order of the list. Group names are unique within a model with active objects (see [Names](model-interactions.md#names)), so for a group the first node is the only one.

### Meshes

- A mesh is a list of triangles: `positions` and `normals`, one normal for each position, and `indices`, three per triangle.
- Each mesh has the index of its `material`.
- Points and lines are left out on import.

### Materials

- Materials follow the metallic/roughness model of glTF 2: a `base_color` in linear RGB with the opacity as its alpha, a `metallic` factor and a `roughness` factor.
- A format with a diffuse color and a separate opacity is read into the same fields.

### Lights

- Each light has its `name`, which is the name of its node, and the index of that `node`. The light shines along -z of its node.
- `type` is the light type of `gfx::light`: point, directional, spot or area.
- `color` is in linear RGB, with the intensity folded in, as Assimp gives it.
- `range` is optional.
- Spot lights have their `inner_cone` and `outer_cone`, measured from the direction of the light to the edge of the cone.
- Area lights have their `shape`, `width`, `height` and `spread`. Squares and disks come back from a file as rectangles and ellipses with both sides the same.

### Active objects

Each entry of `objects` is a loaded active object (`interactions::loaded_object`) with what it works on, found once, at loading:

- `nodes` has one entry for each of `object.groups`: the index of the node of that group, or nothing if the model has no node of that name.
- `light` is the index of the light a light source describes, or nothing for a geometry-based object, or a light source whose light is missing.

`resolve(loaded)` fills `objects` from the loaded objects, finding the groups among the nodes and the lights among the lights, by name. After that, nothing has to look a name up while the model is drawn or animated.

## Units and precision

- Every length is in meters and every angle in radians, the units of the [model space](model-interactions.md#model-space). glTF 2 is in meters by definition; the importer converts other formats on loading (see [Importing](#importing)).
- The positions, normals, matrices and light parameters are kept in `float`, as the files keep them and the GPUs use them. A model spans meters, not kilometers, so `float` keeps its precision well below a millimeter across it.
- Placing models in a scene is where larger distances add up. That composition is the concern of the scene, not of the loaded model, and can be done in `double` before the result is handed to the GPU.

## Drawing a placement

`world_matrices(moves)` returns the matrix of every node in the space of the model, in one pass over the nodes:

```text
world[i] = world[parent(i)] · local[i] · M[i]
```

- `moves` has one matrix `M` per node, the moves of one placement put together from the current values of the channels (see [Drawing](model-interactions.md#drawing)). Each move applies on the local side of its node, around the node's own origin (see [Model space](model-interactions.md#model-space)).
- With no moves, every `M` is the identity, and the matrices are those of the model at rest.
- A placement keeps its moves in a list indexed as `nodes` is, so the loaded model itself is never changed.

`bounds(moves)` returns the box around the vertices of every mesh, in the space of the model, with the nodes placed by `world_matrices(moves)`. A model with no meshes has an empty box.

## Importing

```cpp
struct imported_model {
    gfx::loaded_model model;
    std::vector<interactions::note> notes;
};

imported_model import_model(std::filesystem::path const& path);
```

`import_model` reads a model in any format Assimp reads, with the importer of `pyxel-io` for glTF 2, which adds the area lights and the active objects. It throws `std::runtime_error` if the file cannot be read.

- **Post-processing.** The importer splits the faces into triangles, generates the normals a mesh is missing, and validates the scene. It uses no step that merges or removes nodes (see [Keeping the named nodes](model-interactions.md#keeping-the-named-nodes)).
- **Units.** FBX files are converted to meters, and Collada files are scaled by their own unit. Other formats with no unit of their own are taken to be in meters.
- **Lights.** Each light goes to the first node of its name, the same node Assimp places it by. A light with no node of its name is left out, as there is nothing to place it by. The range, and the shape and spread of an area light, come from the extras of that node.
- **Active objects.** Only glTF 2 files carry active objects (see [File persistence](model-interactions.md#file-persistence)). The importer reads them, checks them against the rules and resolves them against the model. A model in another format has no geometry-based objects, but each named light is still a light source.
- **Notes.** `notes` lists what the importer did to the active objects other than reading them as they are: the ones it dropped, by a rule or by their version, and the ones it upgraded or read in part. The viewer tells the user about them (see [The reading side](model-interactions.md#the-reading-side)).

## Why flat lists

- **Indices instead of pointers.** A placement keeps its matrices, its moves and the storage of its objects in lists indexed as the lists of the model are. An index stays valid when the model is copied or moved, a pointer does not.
- **Depth-first order.** With every parent before its children, the world matrices come from one loop with no recursion and no stack, and a subtree is a contiguous range of the list starting at its root.
- **Names resolved once.** Group and light names are looked up when the model is loaded, so the per-tick work of animating and drawing a placement uses indices only.
