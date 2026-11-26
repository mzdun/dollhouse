# Threads

Until now, models were loaded on the thread the 3D view and the GUI run on. This document proposes a loader thread, which hands the imported model and its diagnostics over to the render thread.

The proposal:

## Render (main) thread

1. On `init(argv)`, if there is a path, posts a load request to the loader and starts the watcher (`watcher::init()`).
2. Is ready to receive a "load in progress" notification. It does nothing for now; later it will start an animation and freeze the state for the reload.
3. Is ready to receive a new model. For now, it only moves the camera and updates the title. Later, it will thaw the state and keep, drop or rebuild each part of it (slots and states, but also the sun and the camera's yaw, pitch and pivot point), then render the model with the current setup (camera, sun, the model's own lights, and the configuration: a grid plane, some artificial walls).
4. If the loader reports a failed load, shows the diagnostics and keeps the previous model.
5. May post a load request at any time, not only at start-up: a "Reload" menu item posts the same request the watcher does.
6. On shutdown, stops the watcher first, then the loader (`watcher::shutdown()`, then `loader::shutdown()`), as the watcher posts to the loader.

### Handing the model over

The render thread never waits for the loader. The current model sits behind a critical section, which the render thread holds for the duration of a frame. When a new result is ready, the loader waits for the current frame to end, takes the lock, exchanges the model for the new one and releases the lock; the next frame locks it again and finds the new model. The loader holds the lock only for the exchange, never during an import.

The "load in progress" notification is an atomic flag the render thread reads at the start of each frame. The render thread draws every frame anyway, so nothing needs to wake it up.

## Loader thread

1. Always runs; sleeps, checking the stop token whenever it wakes up.
2. The first task is the model given on the command line (from `application::init`); after that, the loader only wakes up when a request is posted.
3. Requests come from the render thread (the first one, a reload from the menu) and from `pyxel::watcher`.
4. Imports the model and keeps the model, the diagnostics and the list of resource files, each with the stats it had when it was read (see [Missed changes](#missed-changes)).
5. Hands the result over to the render thread. A result is one of:
   - **loaded**: the model replaces the current one; the import notes go to the log;
   - **loaded, but empty**: a valid file with nothing in it is still a model, and it replaces the current one, like any other;
   - **failed**: there is no model, only the error message; the render thread keeps the old model, if there is one, or keeps showing nothing after a failed first load.
6. Hands the resource list over to the watcher: after a failed import, the new files are added to the last list; after a clean one, the list is replaced. The watcher always gets the full list of files of interest at the moment.

### Requests while loading

Requests do not queue up. A request that comes in while an import is running sets a "dirty" flag, and when the import ends, the loader starts one more. Ten saves during one long import cost two loads, not ten.

### Shutting down

An Assimp import cannot be interrupted. The loader checks the stop token between imports, so `loader::shutdown()` waits for a running import to end, drops its result and returns. With a large model, closing the application may take that long.

## Watcher thread

The watcher gets a list of files and wakes the loader up when one of them changed and the changes have settled. How it learns about the changes is up to its backend. Both kinds of backend run on a thread of their own:

- the **polling** backend sleeps between checks of the file stats;
- a **native** backend (inotify on Linux, `ReadDirectoryChangesW` on Windows, FSEvents on macOS) waits for the system to report a change in the directories of the files. It watches directories, not files: most editors save by writing a temporary file and renaming it over the old one, which a watch on the file itself does not survive.

With the polling backend:

1. Always runs; sleeps, checking the stop token whenever it wakes up.
2. When the list changes, takes the stats of each file from the list, as the loader recorded them.
3. Wakes up periodically and compares the file system stats with the ones it has. For each file, it keeps the stats of the last import and the stats at the last change, nothing older.
4. If nothing changed, goes back to sleep.
5. If any of the files changed, waits for the writes to settle (see [Half-written files](#half-written-files)), then wakes the loader up (which sends the "load in progress" notification, loads the model, updates the watch list and hands the model over), and goes back to sleep.

## Messages between the threads

The loader and the watcher each have a message queue. A thread posting a message locks the queue, adds the message and releases the lock. The owner of the queue, in its `poll()`, takes the whole queue at once, by a `std::swap` with an empty one under the lock, and then handles the messages with the lock released, checking the stop token between messages.

No thread calls into another while holding a lock of its own: the loader posts the new list to the watcher, and the watcher posts a request to the loader. The only lock two threads wait on directly is the model's critical section, described in [Handing the model over](#handing-the-model-over).

## Problems to solve

### Missed changes

If the watcher took the stats of a new file when the list reached it, it could miss a change:

1. the loader reads `door.bin`;
2. the user saves the model again;
3. the watcher gets the new list and takes the stats of `door.bin`, already of the new version, so it never sees a change.

So the stats are taken while the files are read: the `Assimp::IOSystem` wrapper records the modification time and the size of each file it opens, and the scan for external textures does the same. The list says which version of each file the model was built from, and the watcher compares the files against that version, not against what it finds when the list arrives.

### Half-written files

An exporter writes the `.gltf` first and the `.bin` after it. A reload started by the first change could read a new `.gltf` with an old or half-written `.bin`. So the watcher waits for the changes to settle: it reports a change only once the stats of the changed files stay the same for one more check (or for about 200 ms, with a native backend).

## Later

### More than one model

For now, the loader serves the Inspector and one model. Later, it will serve every load of the Dollhouse as well, one at a time, on this thread, with a hand-off after each step: the plan, with no models yet; then the model with the id "door"; then the one with the id "balcony door"; and so on, each shown as soon as it is ready.

### More stages

Nothing decodes the images or uploads the geometry to the GPU yet. Once something does, it will run on the render thread at first and cost frame time for large models. Later, the pipeline may get more stages, off the render thread, maybe each on a thread of its own:

```text
[render: init] / [watcher: reload]
  -> [import: Assimp]
  -> [decode: SDL_image]
  -> [upload: GPU buffers]
  -> [draw: the new geometry]
```
