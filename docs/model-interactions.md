# Model interactions

## Motivation

Interactions have two goals: letting the user manipulate a model, and letting one model influence others.

The user can press E to open a door or a window, flip a switch or pull out a drawer. The model then changes some of its parts, for that one instance in the scene only: some meshes rotate, others move.

The parts of a model can depend on each other. A balcony door can have one tilt-and-turn wing, while the other wing only opens, and only when the first one is open, not tilted.

Finally, when a light switch is turned on or off, there has to be a way to find the light source among the other models in the scene and turn it on or off as well.

Interactions are stored in glTF 2 files only: other file formats cannot carry them inside the model file. For more, see [File persistence](#file-persistence).

## Terms

- **Action.** A labelled request to change the state, such as "Tilt" or "Open both", identified by its position in the list of actions. Which state it leads to depends on the current state; an action whose `when` does not name the current state is not available. The default interaction is not an action of its own (see [Actions](#actions)).
- **Active object.** A named part of a model the user or other models can interact with. It brings together group names, states, actions and slots. A model can define any number of active objects. A model loader lets its user both enumerate them and access them by name, so their names should be unique within the model; for the objects describing lights, they must be (see [Names](#names)).
- **Change duration.** The time it takes all changing channels of an active object to reach their future values: the longest sum of delay and duration among their transitions (see [Instantiation](#instantiation)).
- **Channel.** A single value that states set and transitions animate: one transformation of a group (`rotate.x`, `translate`, ...), an output slot or a variable, or the intensity factor of a light source, calculated from its `enabled` and `intensity` input slots.
- **Default interaction.** The interaction an application binds to a single input event, such as E, pad A or a double-click on a highlighted group. It follows the first action available in the current state (see [Actions](#actions)).
- **Default value of a channel.** The value a channel has when no state sets it: 0 for a translation or a rotation, 1 for a scaling, the slot's default value for a slot, 1 for the intensity factor of a light source (see [Instantiation](#instantiation)).
- **Future value.** The value the current state sets on a channel, or the default value of the channel if the state does not set it. The channel reaches it once its transition finishes.
- **Group name.** The name of a node, unique in the whole model. It points to the node and everything below it, so that transformations can be applied to that part of the model.
- **Light source.** A named light in a model, treated as a special active object without being declared as one. It has predefined input slots and no declared user-facing states or variables.
- **Net-zero change.** A change of a channel between two states that leaves its value the same: both states set the channel to the same value, or neither sets it, so it stays at its default value. A group moves between two states if at least one of its changes is not net-zero.
- **Progress.** A _factor_ telling how far a change of state has advanced, from 0 when it starts to 1 when every channel has reached its future value. The timing function of each channel gets its own input derived from it (see [Instantiation](#instantiation)).
- **Slot.** A typed value an active object exposes to other models: an input slot can be set from outside, an output slot can be wired to input slots of other models, and a variable slot is both.
- **State.** A named set of changes, each setting a value on one channel of a group, an output slot or a variable.
- **State highlights.** The groups an application may highlight while an active object is in a given state: the union of the groups that move between this state and any state one of its actions leads to (see [Actions](#actions)).
- **State machine.** A map from each state to the actions available in it, and from each action to the state it leads to.
- **Storage.** The data kept for one placement of a model in a scene, for each of its active objects. The storage holds at least the name of the current state, the progress of the change toward it, and the previous and future values of the channels.
- **Transition.** Either of two things:
  - a CSS-like declaration of how a change of one channel unfolds: its duration, timing function and optional delay;
  - the act of moving channels from their current values to their future values over time.
- **Update expression.** A formula, algebraic or logical, over input slots and variables (including those synthesized for a light source) that produces a new value for a channel.
- **Variable.** A bidirectional slot, declared with the `variable` direction. It keeps a typed value inside an active object, and, being both an input and an output slot, it can be wired on either side of a wire.
- **Version.** The version of this specification an active object is written for, such as `"1.0"` (see [Versions](#versions)).
- **Wire, wiring.** A many-to-many connection between slots.

## Model space

Interactions are declared in the **model space**, the ordinary three-dimensional space the model is written in:

- The X axis points right.
- The Y axis points up.
- The Z axis points toward the camera, out of the model's front.
- Positive rotations follow the right-hand rule: looking from the positive end of the axis toward the origin, they turn counterclockwise.
- Lengths are in meters and angles in radians, as everywhere in glTF. The text of this specification gives angles in degrees where it reads better, such as 90° for π/2; the JSON always holds radians.

A group's transformations apply on the local side of its node, in the node's own axes and around the node's own origin: `local' = local · M`, where `M = M1 · M2 · ...` composes the transformations in the order they are listed (column vectors, as in Assimp and GLM). Each transformation turns or moves the axes already turned or moved by the ones before it, so `rotate.x` followed by `rotate.y` differs from `rotate.y` followed by `rotate.x`.

## Active objects

### Names

The name of a geometry-based object should be unique in its model. An active object named after a light of the model describes that light instead (see [Putting it together](#putting-it-together)), so a geometry-based object cannot take the name of a light. Names of active objects, lights and slots must not contain a dot (`.`) or a slash (`/`): a project placing the model may use these characters to separate the parts of a reference, such as `"<node>/<object>.<slot>"` in the [plans schema](plans-schema.md#wiring-objects-together).

Two lights of a model must not share a name, and two active objects must not describe the same light. The exporter fails on such a model, and the importer drops the active objects of all the lights sharing the name, or all the objects describing the same light, telling the user (see [The reading side](#the-reading-side)). The lights themselves still shine, at the default values of their slots.

A light is exported on a node of its own, named after the light, so the names of lights and the names of groups are all node names of the model: a group must not take the name of a light. On import, the name of a light is the name of the node it is attached to.

A light with an empty name is not an active object. It cannot be wired or described by an active object, and it is always on, at full intensity, with no transition. Two such lights do not share a name in the sense above.

A model in which geometry-based objects share a name is still usable: a reference to that name reaches the object declared first in the model (see [The reading side](#the-reading-side) for the order of objects merged from several nodes).

Inside the declaration of an active object, in its `states`, `updates` and `transitions`, a slot is referred to by its declared name prefixed with a dollar sign: the slot `enabled` is `$enabled`. Every key starting with `$` refers to a slot, so a group whose name starts with `$` cannot be used by an active object. A slot whose declared name itself starts with `$` is referred to with one more dollar sign: the slot `$enabled` is `$$enabled`. Outside the declaration, for example in a wiring, a slot is always referred to by its declared name, without the added dollar sign.

### Geometry-based objects

A geometry-based object is any active object that is not a light source. It can operate on any number of groups, transforming their local matrices. A well-described object declares the states it can be in and the actions that move it between them.

This version defines no predefined slots for geometry-based objects. The slot name `visible` is reserved: a future version may add a predefined `input boolean visible` slot to show and hide the groups the object manages. Declaring a slot named `visible`, of any type and in any direction, is an error.

### Light sources

Every compatible importer exposes each named light in the model as an active object and attaches two predefined input slots and one internal variable to it:

- `input boolean enabled` turns the light on and off, for light switches. Its default value is `true`, so that a light not wired to any switch still shines: a light source has no states or actions of its own, and nothing else could turn it on.
- `input factor intensity` sets how bright the light is, for dimmers. Its default value is 1.
- an unnamed `variable factor` keeps the result of the update expression calculated from both input slots. This specification leaves it unnamed on purpose, and a compatible application must not allow wiring it in any way.

A light source has no declared states and no user-facing variables. A transition can be declared on a light source; it applies to the unnamed intensity factor.

The update expression of a light source is `enabled ? intensity : 0`: the intensity factor follows `intensity` while the light is enabled, and is 0 otherwise. The light shines with the intensity declared in the model multiplied by the current value of its intensity factor.

The update expression is evaluated once per tick, after all input slots of the light have received the signals sent to them in that tick (see [Order within a tick](#order-within-a-tick)). Its result is the future value of the intensity factor, and the transition runs whenever this future value differs from the current value. Input changes that leave the result the same trigger nothing: `enabled` turning on in the same tick as `intensity` is set to 0, or `intensity` changing while `enabled` is off. `enabled` turning on while `intensity` is above 0 changes the future value and starts the transition.

To keep the rest of this text simple, every change of the future value of the intensity factor is treated as a request for a synthesized state of the light source, which sets the factor to that future value with a `set` transformation. The transition of the factor then starts from its current value, as for any other request (see [Changing states](#changing-states)).

### States

`states` is a two-level mapping from state names to state declarations.

On the first level, the order of the keys matters: the first state in `states` is the starting state of the active object (**start**, see [Actions](#actions)), unless the application chooses another one. The importer must keep the order of the JSON, for example in an ordered map, and the exporter must write the states in the order they were declared. A reader must not rely on the objects of its language for that: in JavaScript, for one, `JSON.parse` moves integer-like keys, such as a state named `"1"`, to the front of an object, so a reader there has to turn `states` into a `Map` from the text, or read it with a parser that keeps the order.

On the second level, each state maps group names and slot references (`$` and the slot name, see [Names](#names)) to the list of transformations that put this group or slot in this particular state. The mapping itself is unordered, but each list is ordered by nature.

A list names each channel at most once. Across all states of an active object, the transformations of one group keep a single order: the order in which its channels first appear, going through the states in the order they are declared. The list of each state names some of these channels, in that order; a state listing two channels of a group in the opposite order to another state is an error. When the group is drawn, its transformation composes all of its channels in that order, in every state and at every moment of a transition, and a channel the state does not set contributes its default value.

More than one active object may move the same group. Models are discouraged from doing so, but, as with overlapping highlights (see [Actions](#actions)), this specification does not forbid it. The transformations of the objects are then composed in the order the objects are declared: `local' = local · M1 · M2 · ...`, where `M1` is the transformation of the group put together by the first of these objects, `M2` by the second, and so on.

At every animation tick of a running transition, a new value is calculated for every channel in the channel list of the object (see [Changing states](#changing-states)). The new value either updates the geometry and lights in the scene or is sent along the wires of a slot.

State names are free-form, but some of them, such as `on`, `off`, `yes` and `no`, are booleans in YAML 1.1 and need quotes there. Names such as `switched-on` and `switched-off` avoid the problem.

#### Channels

A group has these channels; each item of its list of transformations sets exactly one of them:

| Channel | Value | Default value |
| ------- | ----- | ------------- |
| `translate.x`, `translate.y`, `translate.z` | _length_ | 0 m |
| `translate` | one to three _lengths_, for X, Y and Z; a missing one is 0 m | 0 m on every axis |
| `rotate.x`, `rotate.y`, `rotate.z` | _angle_ | 0 |
| `rotate` | one to three _angles_, for X, Y and Z, applied in this order; a missing one is 0 | 0 on every axis |
| `scale.x`, `scale.y`, `scale.z` | _float_ | 1 |
| `scale` | one to three _floats_, for X, Y and Z; a missing one is 1 | 1 on every axis |

Any scale is allowed, negative or zero included. A negative scale mirrors the group along its axis, turning its faces inside out, and a zero scale flattens the group along its axis. How visual artifacts of a group flattened along one or more axis is drawn is left to the viewer.

`rotate: [x, y, z]` is the same as `rotate.x: x`, `rotate.y: y` and `rotate.z: z` listed in this order, following [Model space](#model-space). It matches `glm::eulerAngleXYZ(x, y, z)`, which builds `Rx · Ry · Rz`.

An output slot or a variable has a single channel, `set`, taking a value of the slot's type. The default value of the channel is the default value of the slot. An input slot has no channel: it is read-only for states, and a state setting it is an error. A variable set both by a state and by a wire takes the value of whichever update came last; within one tick, the state comes after the wire (see [Order within a tick](#order-within-a-tick)).

A light source has a single channel, its unnamed intensity factor (see [Light sources](#light-sources)). It cannot be set by a state, but a transition can be declared on it.

The group channel name `visible` is reserved for a future version, to show and hide a group, as in KHR_node_visibility. A state must not use it in this version.

The shape of the decoded `states` is given by the [JSON Schema](#json-schema).

### Actions

In JSON, `actions` is a list that defines both the actions the user can take on an active object and its state machine, in which the actions are the edges between states. An action has no name of its own; it is identified by its position in the list. Each entry has:

- `label`, the text shown in the UI. It is required, but does not have to be unique.
- `when`, the name of the state the action leaves, or a list of them.
- `then`, the name of the state the action leads to.

Every state named in `when` or `then` must be declared in `states`.

An action can only be taken along an edge of the state machine. Requesting an action that is not available in the current state, one with no edge from that state, leaves the object where it is, as if the edge led back to the current state: nothing changes, and a running transition goes on.

When loaded, an active object holds:

- **start**, the first state in `states`, or none for an object with no states. It is the preferred starting state: an application may use it directly, or as one of the steps that decide the starting states of a placed model.
- **actions**, the labels of the actions, in the order of the JSON, for an application to show as options in its UI.
- **machine**, which maps each state named in `when` or `then` to its outgoing edges: a mapping from an index into **actions** to the target state. The index `-1`, out of bounds for any list, stands for the default interaction (see below).
- **state highlights**, which maps each state to the groups an application may highlight while the object is in that state. For a state, it is the union, over every state its outgoing edges lead to, of the groups that move between the two states (see [Net-zero change](#terms)).

The default interaction (E, pad A, or a double-click on any group in the state highlights of the current state) follows the first action in the list whose `when` names the current state, and the `-1` edge records where it leads. A state with no outgoing actions has no `-1` edge and no highlights, so the default interaction does nothing in it. An application building an options menu from **machine** skips the `-1` edge, as it has no label.

The current highlights of several active objects may overlap, so that a clicked part of the model lies in or below highlighted groups of more than one object. Models are strongly discouraged from doing so, but this specification does not forbid it. The UI may combine the highlights and the options of all the objects involved, and it is left to the application to decide what the default interaction changes. This specification suggests following the `-1` edge of every object involved.

An action moves the object from one state to another, so it cannot set a value in between. A future version may add continuous actions for that: while the user holds the input down, such as a long press of the mouse button, E or pad A, the action keeps changing a single channel within a declared range, for example `rotate.z` from −90° to 90° for the knob of a dimmer, and the object stays at the value reached when the input is released. Defining how such a value is kept in the storage, and how it relates to the states, is left to that version.

#### Actions examples

```yaml
- name: tilt-and-turn
  version: "1.0"
  states:
    closed: {}
    tilted: {tilt-hinge: [{rotate.x: 0.0873}]}   # 5°
    opened: {right-hinge: [{rotate.y: 1.3963}]}  # 80°
    opened-wide:
      right-hinge: [{rotate.y: 1.3963}]          # 80°
      left-hinge: [{rotate.y: -1.3963}]          # -80°
  actions:
    - {label: Tilt, when: closed, then: tilted}
    - {label: Open one, when: closed, then: opened}
    # before "Close", so that the default interaction
    # in "opened" opens the second wing
    - {label: Open both, when: opened, then: opened-wide}
    - {label: Close, when: [tilted, opened], then: closed}
    - {label: Close left, when: opened-wide, then: opened}
```

When loaded, this object holds:

```yaml
start: closed

actions:
  - Tilt
  - Open one
  - Open both
  - Close
  - Close left

machine:
  closed:
    -1: tilted
    0: tilted
    1: opened
  tilted:
    -1: closed
    3: closed
  opened:
    -1: opened-wide
    2: opened-wide
    3: closed
  opened-wide:
    -1: opened
    4: opened

state highlights:
  closed: [tilt-hinge, right-hinge]
  tilted: [tilt-hinge]
  opened: [right-hinge, left-hinge]
  opened-wide: [left-hinge]
```

### Transitions

`transitions` declares how the channels of an active object change over time while the object moves between states. In JSON, it is a mapping from a target to a transition value. A target is one of:

- a group name;
- a reference to an output slot or a variable, `$` and the slot name (see [Names](#names));
- `*`, the defaults for every target of the object; a target listed by name overrides them channel by channel;
- `light`, the unnamed intensity factor, in an active object describing a light source (see [Putting it together](#putting-it-together)). In a geometry-based object, `light` is an ordinary group name.

A transition value is either:

- `none`, for no transition: the channels of the target take their future values at once. This is the default.
- a comma-separated list of single-channel transitions.

A single-channel transition has up to four parts, each of them optional:

- the channel it applies to: a channel name, such as `rotate.x`, or `all` for every channel of the target that changes. Without one, `all` is assumed.
- a timing function, from the table below;
- up to two times: the first one is the duration, the second one the delay. A time is a number followed by `s` or `ms`, as in CSS.

The parts may come in any order, but the first time is always the duration. An omitted part takes its value from **`all 0s ease 0s`**.

When more than one item of the list applies to a channel, the last one wins. A channel named after `all` overrides it: with `all 200ms ease-in, rotate.x 200ms ease-out`, `rotate.x` eases out and every other channel eases in.

A target the object cannot change is an error: a group the model does not have, an undeclared slot, or an input slot. So is a transition value that does not parse, including a channel its target does not have, a time without a unit, or more than two times. A negative duration or delay is not an error: it is taken as 0s, as time cannot run backward.

Unlike CSS, transitions have no `allow-discrete` keyword. A boolean channel, such as a `boolean` slot, is never interpolated: it jumps to its future value once the delay and the duration have passed. Every other channel is interpolated, as with `transition-behavior: normal` in CSS; an _offset_ moves along all three axes at the same time.

Rotations are interpolated as angles, never as quaternions or matrices: each angle of `rotate.x`, `rotate.y`, `rotate.z` and `rotate` moves from its previous to its future value on its own, and the angles become a rotation only when the transformation of the group is put together for drawing. A quaternion cannot tell a turn of 270° from a turn of −90°, so interpolating one would always take the shorter way, and a revolving door or a dial turning from −90° to 90° could turn the wrong way. Angles keep the direction the model declares.

Only the timing functions below are supported. `cubic-bezier()` itself cannot be used, but each supported function is defined [in terms of it](https://www.w3.org/TR/css-easing-1/#cubic-bezier-easing-functions).

| Function | Equivalent to |
| -------- | --- |
| `ease` | `cubic-bezier(0.25, 0.1, 0.25, 1)` |
| `ease-in` | `cubic-bezier(0.42, 0, 1, 1)` |
| `ease-out` | `cubic-bezier(0, 0, 0.58, 1)` |
| `ease-in-out` | `cubic-bezier(0.42, 0, 0.58, 1)` |
| `linear` | `cubic-bezier(0, 0, 1, 1)` |

#### Transitions grammar

A transition value follows the [`transition` shorthand](https://www.w3.org/TR/css-transitions-1/#transition-shorthand-property) of CSS Transitions Level 1, written here in the [value definition syntax](https://www.w3.org/TR/css-values-3/#value-defs) of CSS Values and Units Level 3, the companions of the [CSS Easing Functions Level 1](https://www.w3.org/TR/css-easing-1/) used for the timing functions:

```text
<transition-value>  = none | <single-transition>#
<single-transition> = <channel> || <time> || <easing-function> || <time>
<channel>           = all
                    | translate | translate.x | translate.y | translate.z
                    | rotate | rotate.x | rotate.y | rotate.z
                    | scale | scale.x | scale.y | scale.z
                    | set
<easing-function>   = ease | ease-in | ease-out | ease-in-out | linear
```

- `||` means that each part appears at most once, in any order, and `#` a comma-separated list of one or more items. Of the two `<time>` parts, the first one is the duration, and the second one the delay.
- `<time>` is the [`<time>`](https://www.w3.org/TR/css-values-3/#time) of CSS Values and Units Level 3: a number with the unit `s` or `ms`. Tokens, numbers and whitespace follow [CSS Syntax Level 3](https://www.w3.org/TR/css-syntax-3/), so `.5s`, `1e3ms` and `+2s` are times, and `0` is not. Keywords and units are ASCII case-insensitive, as in CSS.
- Which channels belong to which target: a group has all of them but `set`; an output slot or a variable, the intensity factor of a light source has `set`. Naming a channel the target does not have is an error, except under `*`, where such a channel is skipped for the targets without it.

The differences from CSS:

- `none` stands only for the whole value. CSS also allows it inside a list, as the property of one item.
- Channels take the place of properties, and their names contain a dot.
- The timing functions are the keywords above only: `cubic-bezier()`, `steps()` and the `linear()` function cannot be used.
- There is no `transition-behavior`, and so no `allow-discrete` (see above).
- A negative duration or delay is taken as 0s. CSS rejects a negative duration and starts a transition with a negative delay partway through.

#### Transitions examples

```yaml
- name: tilt-and-turn
  version: "1.0"
  # states and actions as in the actions examples
  transitions:
    # all three groups of this object get 200 ms ease-in
    # for rotate.x and rotate.y by default...
    "*": rotate.x 200ms ease-in, rotate.y 200ms ease-in
    # ...but "tilt-hinge" takes 300 ms for rotate.x,
    # keeping rotate.y from "*"
    tilt-hinge: rotate.x 300ms ease-in

- name: door
  version: "1.0"
  # states and actions left out
  transitions:
    # every channel of "hinge" eases in over 200 ms,
    # except for rotate.x, which eases out
    hinge: all 200ms ease-in, rotate.x 200ms ease-out

- name: switch
  version: "1.0"
  slots:
    - {name: enabled, type: boolean, direction: output}
  states:
    switched-off: {}
    switched-on:
      rocker: [{rotate.x: 0.1745}]  # 10°
      $enabled: [{set: true}]
  actions:
    - {label: Turn on, when: switched-off, then: switched-on}
    - {label: Turn off, when: switched-on, then: switched-off}
  transitions:
    rocker: rotate.x 50ms
    # duration, timing function, delay: the boolean value
    # flips after 45 ms, as the rocker passes its middle
    $enabled: 0ms linear 45ms

# an active object describing a light source declares
# nothing but the "light" transition: every change of
# the light's intensity eases over 150 ms
- name: light-bulb
  version: "1.0"
  transitions: {light: 150ms}
```

### Putting it together

Every active object has a `name` and a `version` (see [Versions](#versions)).

If the name is the name of a light in the same model, the active object describes that light. Apart from these two, its only property is `transitions`, and its only transition target is `light`. An entry describing a light that also has `slots`, `states` or `actions` is an error: the exporter fails on it, and the importer drops the entry, telling the user (see [The reading side](#the-reading-side)). No two active objects may describe the same light (see [Names](#names)).

Otherwise, the active object is geometry-based, and no two such objects should share a name (see [Names](#names)). It can have `slots`, `states`, `actions` and `transitions`. None of them is required, but in this version an object with no states and no actions offers nothing to interact with. Neither does an object whose actions move no group, such as one whose states set only slots: its states have no highlights, so the user has nothing to click or aim at.

The shape of the whole active object is given by the [JSON Schema](#json-schema), and in C++ by the [C++ data structures](#c-data-structures).

### Versions

Every active object, including one describing a light, has a required `version`: the major and the minor version, separated by a dot, each written as in SemVer, with no leading zeros. The whole string matches `(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)`, so `"1.0"` and `"1.12"` are versions, and `"1"`, `"01.0"` and `"1.0.0"` are not. The two numbers have the meaning of [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html), with the patch version always 0: `"1.0"` stands for `1.0.0`. This specification defines version `"1.0"`, and the exporter writes it on every object. In YAML, the version needs quotes, as `1.0` alone is a number.

As in SemVer, a new minor version only adds features, and an object valid in one minor version stays valid, with the same meaning, in every later minor version of the same major version. A new major version may be incompatible with the earlier ones. It may define an upgrade path from them, but nothing in this specification can enforce that.

An importer reading an object of a version other than its own acts on the difference:

- **Older minor version, same major version.** The object is read as it is.
- **Newer minor version, same major version.** The importer reads the object as one of its own version, and ignores what it does not know, dropping the smallest item that holds it:
  - an unknown key of an active object, a slot or an action: the key and its value;
  - an unknown slot type or direction: the slot, together with every reference to it in `states` and `transitions`;
  - an unknown channel in a list of transformations: that transformation;
  - an unknown channel or timing function in a transition value: that single-channel transition, keeping the rest of the value.

  The same names are errors in an object of the importer's own minor version or an older one. The importer may tell the user what it ignored. If what remains breaks a [rule](#rules), the object is dropped as any other.
- **Older major version.** If the importer knows an upgrade path from that version, it upgrades the object, and tells the user that the object was written for an older version and was upgraded. Otherwise, it drops the object.
- **Newer major version.** The importer drops the object.

An object with no `version`, or with one that does not match the pattern above, is dropped as well. The [rules](#rules) are checked on the object as read, upgraded or stripped of what was ignored. For every dropped object, the user must be told which one and why (see [The reading side](#the-reading-side)).

## Signal propagation

### Slots

`slots` holds a list of slot declarations. Each slot must have a name, and, so that it can be referenced from outside the model, the name must be unique within its active object and follow the rules in [Names](#names).

Each slot has a type understood by the pyxel engine (including the shapes API). This specification defines:

- **float**, a double-precision floating-point number.
- **factor**, a _float_ clamped to the range [0, 1]. Like any two slots of different types, an output _float_ slot cannot be wired to an input _factor_ slot (see [Wiring](#wiring)).
- **boolean**, a two-valued type, mapped to `bool` in both Python and C++.
- **angle**, an angle in radians, for example for rotation transformations. Positive angles turn as described in [Model space](#model-space).
- **length**, a distance in meters along one of the axes, for example for translation transformations.
- **offset**, a list of three _lengths_, the distances along the X, Y and Z axes of the model space. A future specification may define how to build an _offset_ from _length_ slots or how to extract a single axis from it; this one only defines the type.

The type names `int` and `color` are reserved for future versions: `int` for multi-position switches, counters and choosing one of several values (as in KHR_interactivity), and `color` for animating the color of a light or the emissive color of a material (as with KHR_animation_pointer). A slot must not use any of them in this version.

Each slot also has a direction: input, output, or a third direction called "variable" (see below).

A variable slot, or simply a variable, is bidirectional: it is both an input and an output slot, so it can be used to relay a signal. Variables also take part in [update expressions](#update-expressions), which are out of the scope of this version.

A slot may declare a default value. If it does, the value **must** be one the slot's type accepts. If it does not declare one, the default value is the default-initialized value of the slot's type: `false` for `boolean`, zero (or a value constructed from zero) for `float`, `factor`, `angle` and `length`, and the equivalent of `[0, 0, 0]` for `offset`. The default value is used to initialize the storage of the active object.

The shape of a slot declaration is given by the [JSON Schema](#json-schema).

### Wiring

An output slot can be wired to an input slot; a variable slot can stand on either side of a wire, as it is both. The two slots of a wire must have the same type. A future version may offer a way to connect slots of different types, for example by clamping a _float_ into a _factor_ on the way. Wires carry updates from output slots to input slots. Wires may form a cycle, by accident or by design, so the consequences a slot change has on other models cannot be calculated within a single frame. For Dollhouse, [Several outputs to one input](plans-schema.md#several-outputs-to-one-input) in the plans schema addresses this.

A wire must state unambiguously which way the signal flows. This matters most for variable slots. In Dollhouse, for instance, the node that places a model with output slots is responsible for wiring them to input slots of other models.

An application may offer automatic wiring, for example connecting a light switch to the lights it is placed next to. Authors are advised to name and type their slots so that such wiring can find them: `enabled` for an on/off signal of type `boolean`, `intensity` for a brightness of type `factor`, matching the input slots of a light source (see [Light sources](#light-sources)).

As the wiring is many-to-many, several signals may arrive at one input slot in a single tick. The application must order such signals consistently, as only the last update of a slot in a tick has lasting consequences.

An output slot sends a signal along its wires whenever its current value changes. The change is registered in the tick it happens in, and the wired input slots receive it in the next tick (see [Order within a tick](#order-within-a-tick)).

### Update expressions

In this version of the specification, no syntax is defined, and the only update expression is the one synthesized for each light source. It updates the light's intensity factor, which changes the light the source produces in the scene.

For future use, names starting with a dollar sign are reserved for slot references in update expressions, as in any other reference inside the declaration of an active object (see [Names](#names)).

Likewise, the name `updates` is reserved for declaring update expressions.

#### Example (informal)

The example below uses a syntax that is not part of this specification; it is merely inspired by the C family of languages.

Assume that the unnamed intensity factor variable is named `intensity-factor`. Its update expression could then be declared as follows:

```yaml
  updates:
    # synthesized:
    #   light.intensity = <intensity from the file> * $intensity-factor
    $intensity-factor: "$enabled ? $intensity : 0"
```

Apart from the leading `$` and the reserved word `updates`, nothing in this example is part of the specification.

### Updating an output slot on a state change

To set the value of an output slot or a variable when the state changes, add the slot's reference (its name prefixed with `$`) to the state, with a `set` transformation, as described in [States](#states).

### Interaction between input slots and actions

This specification aims to define, at some point, a way for an input slot to trigger an action and so move the state machine. This version does not define it.

Until then, an input slot of a geometry-based object has no effect on the object. It keeps the value it receives, and a variable sends that value on along its own wires, but no state, transition or group of the object reacts to it. Only the input slots of a light source have an effect, through its update expression.

## Instantiation

A loaded model holds all the declarations of its active objects. A model placed in a scene uses these declarations to create the storage for this particular placement. If a model is placed several times, each placement gets its own storage for the model's active objects.

For each object, the storage holds:

- **State name.** The name of the current state. It is initialized with the starting state chosen by the application, by default **start**, the first state in `states` (see [Actions](#actions)). The state the object is leaving is not kept: a transition only needs the current values of the channels it moves from (see [Changing states](#changing-states)).
- **Progress.** A _factor_ telling how far the state change has advanced, relative to the change duration (see below). It is initialized to `1`: the object starts settled, with no change under way.
- **Channels.** The previous, current and future values of every channel of the object: each channel any state sets, and the intensity factor of a light source. All three are initialized with the value the starting state sets on the channel, or with the default value of the channel if the starting state does not set it.

A new placement thus starts settled in its starting state, with no transition: a door whose starting state is `ajar` is drawn ajar. Its slots then reach the rest of the scene in the [startup phase](#startup-phase).

The change duration is the longest delay plus duration among the transitions of the channels in the channel list (see [Changing states](#changing-states)). The change duration may be cached in the storage, but it does not have to be.

The progress is kept for the change as a whole, but each channel follows its own transition. At any moment, the time elapsed since the change started is the progress multiplied by the change duration, and the timing function of a channel gets the input `clamp((elapsed − delay) / duration, 0, 1)`, with the delay and the duration of that channel's transition. The channel stays at its previous value until its delay has passed, and reaches its future value when its delay and duration have passed, which may be before the change as a whole is complete. A channel with a duration of zero takes its future value once its delay has passed, and so does a boolean channel (see [Transitions](#transitions)).

The current value of a channel lies between its previous and future values, at the point the timing function gives for the current animation time; for a boolean channel, it is one of the two. Caching it is optional, but it can help to build model transformations between animation ticks.

The default value of a channel is one of:

- the declared or implicit default value of a variable or a slot;
- for a group transformation, 0 for a rotation around any axis, 0 m for any translation, 1 for any scaling (see [Channels](#channels));
- for the intensity factor of a light source, the result of its update expression for the default values of its input slots: 1, as `enabled` defaults to `true` and `intensity` to 1 (see [Light sources](#light-sources)).

### Startup phase

Placing the models of a scene and restoring a kept storage both end with a startup phase, run by the loader before the scene is shown: a short burst of emulated ticks, for example five, that follow no clock. Each tick of the burst works like any other tick (see [Wiring](#wiring)): a slot that changes registers the change, the wired input slots receive it in the next tick, and light sources evaluate their update expressions. The only difference is that every transition started during the burst is immediate: its channels take their future values at once, as if every duration and delay were 0s.

The first tick of the burst sends the value of every output slot, variables included, along its wires, as if it had just changed. For example:

- A switch starting `switched-on` sets its `enabled` output slot to `true`, although the default value of the slot is `false`, and sends `true` to the wired lamp.
- A switch starting `switched-off`, either declared first or brought back from a kept storage, sends `false`, and turns off the wired lamp, although the lamp's `enabled` slot defaults to `true`. The lamp is off when the scene is first shown, with no fade.

A signal still under way when the burst ends, at the end of a chain longer than the burst, arrives on the clock, and its transitions run as usual: a lamp at the end of such a chain may still fade in or out after the scene is shown. An application may size the burst to the scene instead, for example by finding the longest chain of wires and detecting and breaking the cycles.

### Keeping the storage between runs

An application may keep the storage of a viewed scene between runs, and this specification advises it, for the sake of continuity. Between two runs, the scene may change: another program may edit the scene file, moving a placement to another place, adding a model or removing one, and a model may be rebuilt with a different set of active objects, states or slots. None of this invalidates the storage as a whole. The application restores what still matches, item by item, and creates the rest from the default values, so a light left on stays on, even if the room around it has changed.

To make this possible, the kept storage holds, for every placement:

- the **placement key**, which identifies the placement in the scene;
- for each active object:
  - its **name** and whether it is a light source;
  - the name of its **current state**;
  - for each of its slots, variables included, the slot's **name**, **type** and **value**.

Nothing else is kept. A running transition is not: the restored object starts settled in its current state, as if the transition had finished. The channels are not either, as they follow from the state and the slots.

The placement key is chosen by the application, and decides which edits the storage survives. It must not depend on where the placement is in the scene, so that moving it keeps its storage. In Dollhouse, it is the `name` of the node or opening placing the model (see the [plans schema](plans-schema.md#nodes)). An unnamed node has no stable identity: the application may key it by its place in the node tree, and it then starts from the default values once moved. An application showing a single model, such as Inspector, needs no key at all.

#### Restoring the storage

When a scene is shown again, the storage of every placement is restored from the top down, and each level matches only what the level above it has kept:

1. **Placements.** A placement with a kept storage under its key restores it. A new placement, with no storage kept, starts from the default values. A kept storage whose key matches no placement belongs to a removed model and is dropped. A placement that now places another model still restores its storage, as the next steps match it against the active objects of the new model.
2. **Active objects.** An active object restores the kept one of the same name and the same kind, a light source or a geometry-based object. An active object with no match starts from the default values, and a kept one with no match is dropped. If several geometry-based objects share a name, only the first declared of them restores the kept one, and the others start from the default values (see [Names](#names)).
3. **States.** If the object still declares the kept current state, the object starts in it, with no transition: the state name is set to it, the progress to 1, and the channels to the values the state sets. Otherwise, the object starts in the state the application would choose for a new placement.
4. **Slots.** A slot restores the kept value of the slot with the same name and the same type. A slot with no match, or with a changed type, starts from its default value, and a kept slot with no match is dropped. The slots the starting state sets take the values from the state instead, so that a rebuilt model always agrees with its own declaration.

A light source restores its `enabled` and `intensity` input slots like any other slots. Its intensity factor is then calculated from them, and set at once, with no transition.

Wiring is not kept: it is read again from the scene, which may have changed as well. So, once every placement is restored, the [startup phase](#startup-phase) sends the value of each output slot, variables included, along its wires. An input slot wired to an output then takes the value from the current wiring, and an input slot with no wire keeps its restored value. A lamp still wired to a switch left on is therefore on again, and a lamp now wired to another switch follows that one.

Some applications may let two runs show the same scene at the same time. Depending on how such an application keeps the storage of a scene, the runs may conflict over the stored data. This specification warns about such cases, but gives no advice on how to resolve them.

Internal suggestion: both Inspector and Dollhouse keep the storage between runs. When run again with the same model path (Inspector) or floor plan path (Dollhouse), both should bring the window already open to the front. For Dollhouse, "the same path" means the path of the root include file.

## Changing states

A new state is requested for an active object when the user takes one of its actions or triggers the default interaction, and the state it leads to is taken from the state machine. For a light source, a change of the future value of its intensity factor requests its synthesized state (see [Light sources](#light-sources)).

A transition moves the channels from where they are to where the requested state puts them. It needs the current values of the channels and the requested state, but not the state the object is leaving, so a request is handled the same way whether a transition is running or not.

Placing a model and restoring a kept storage do not request states. The application drives both, and one of their steps brings a state back by setting it directly, with no transition (see [Instantiation](#instantiation) and [Restoring the storage](#restoring-the-storage)). The steps below do not apply to them.

### Starting a transition

When a state is requested, the storage is updated as follows:

- The **current state name** becomes the requested state.
- The **channel list** is calculated anew: it holds every channel of the object whose current value differs from its future value, the value the requested state sets on it, or its default value if the state does not set it. For each of these channels, the previous value takes the current value. If the list is empty, for example when the object is already settled in the requested state, the algorithm continues with [Stopping a transition](#stopping-a-transition).
- The **change duration** is calculated from the transitions of the channels in the list. If it is zero, within an epsilon chosen by the application, the algorithm continues with _Stopping a transition_.
- The **progress** is set to 0.

From then on, every animation tick advances the progress by the time elapsed since the previous tick, divided by the change duration, and calculates the current values of the channels in the list.

### Interrupting a transition

A request that comes while a transition runs interrupts it, and starts a new transition as above. The channels keep the current values the interrupted transition brought them to, and every channel away from its new future value is in the new list, so it moves on from where it stopped: a door told to close while it is still opening turns back from wherever it is.

### Stopping a transition

When the progress reaches or passes 1, the transition stops and the storage is reset:

- The **progress** is set to 1, in case the clock overshot it in the last tick.
- Every **channel** in the list takes its future value exactly, and the list is emptied.
- The **change duration** is set to zero, if the application caches it.

### Order within a tick

Every tick of a scene goes through the same steps, in this order, for all placements at once:

1. **Wires.** The changes registered in the previous tick reach the wired input slots, variables included. Several signals arriving at one slot are applied in the consistent order the application chooses (see [Wiring](#wiring)).
2. **Actions.** The actions requested since the previous tick, by the user or by the application, are taken in the order they were requested, each starting or interrupting a transition (see [Starting a transition](#starting-a-transition)).
3. **Lights.** Each light source evaluates its update expression, and a change of the future value of its intensity factor requests its synthesized state.
4. **Transitions.** Every running transition advances by the time elapsed since the previous tick, its channels take their new current values, and the transitions reaching the end stop. A transition started in steps 2 or 3 of this tick does not advance yet: it starts from progress 0 in the next tick, unless its change duration is zero and it stopped at once.
5. **Outputs.** Every output slot and variable whose current value has changed in this tick, in step 1 or 4, registers the change for step 1 of the next tick.

A value set by a state therefore comes after a value from a wire in the same tick, and a change crosses one wire per tick. The [startup phase](#startup-phase) runs the same steps, with no clock and with every transition immediate.

## File persistence

A model with interactions is exported only to glTF 2, `gltf2` or `glb2`. Exporting it to any other format is an error (`std::runtime_error` from `export_scene`, like other export failures), not a silent drop and not a companion file.

The interactions are stored as the `PYXEL_interactions` extra of the root node: a JSON subtree in the `extras` of that node, holding the list of active objects in the shape given by the [JSON Schema](#json-schema). Assimp does not write it. The `pyxel-io` exporter writes it into the glTF document after Assimp, the same way it adds the lights.

### Why glTF 2

The format must carry data of our own on a node, both ways, and other tools must be able to read it. Of the formats Assimp 5.4.3 exports, only glTF 2 appears to:

| Format | Custom node data written | Custom node data read | Source |
| --- | --- | --- | --- |
| glTF 2 (`gltf2`, `glb2`) | yes, node `extras` | yes, node `extras` | in use: `glb2-pyxel` and `gltf2-pyxel` write the extras and read them back |
| FBX (`fbx`, `fbxa`) | most likely not | likely, as user properties, so import only | Assimp sources |
| Collada | most likely not, beyond `id`/`sid` | most likely not, beyond `id`/`sid` and scene asset info | Assimp sources |
| assbin | likely | likely, but Assimp-only | Assimp sources |
| assxml, assjson | likely, as a debug dump | no importer | Assimp sources |
| OBJ, STL, PLY, 3DS, X, X3D, PBRT, STEP, 3MF, glTF 1 | most likely not | most likely not | Assimp sources |

Only the glTF 2 row is confirmed by running code. The other rows come from reading Assimp's sources, not from tests. A round-trip test, exporting a node with metadata to each format and importing it back, would confirm them. None of them is likely to change the outcome, as the formats that may carry the data are either Assimp-only or import-only.

Other options considered and rejected:

- companion file: a second file to keep in sync, and needed for glTF 2 too if used for everything
- data encoded in node names: exporters rename nodes (FBX, Collada), OBJ flattens the hierarchy
- animation clips alone: carry the motion, but not states, actions or exclusivity; Collada import can merge clips
- app-side registry per model kind: a companion file kept in another place

### Why written after Assimp

Assimp carries the extras of a glTF node in `aiNode::mMetaData`, both ways, but it cannot carry the interactions there:

- **Strings are cut at 1023 bytes.** A string in `aiMetadata` is an `aiString`, which holds at most 1023 bytes (`AI_MAXLEN`) and cuts a longer one silently, on export and on import. The list of active objects, encoded as a single JSON string, outgrows this limit with the first few objects.
- **Arrays do not survive.** A JSON subtree does pass through `aiMetadata`, as nested `aiMetadata` objects, in the order of the file. But the Assimp importer turns an array into an object whose items all repeat the name of the array, and the Assimp exporter writes every nested value as an object. The list of active objects is made of arrays at every level: the list itself, `actions`, `when`, the lists of transformations and the vectors.

So the exporter writes the extra into the glTF document itself, after Assimp has written the file into memory, and the importer reads it from the document itself (see [Flow](#flow)). Assimp still reads the extra on import, but the copy it puts into `mMetaData` is cut or flattened, and nothing uses it.

### Why an extra

As the exporter writes the document itself, the interactions could also go into an extension, `PYXEL_interactions` in the `extensions` of the node, as the area lights go into `PYXEL_lights_area`. An extra fits them better:

- The area lights are a list in the document, which the nodes refer to by index, the pattern of `KHR_lights_punctual`. The interactions are data of a single node, the root of the model, which is what an extra is for.
- An extension has to be listed in `extensionsUsed`. An extra needs no declaration.
- Tools tend to keep extras they do not understand, and show them to the user: Blender imports the extras of a node as custom properties of its object, and can write them back on export. A model edited in such a tool has a chance to keep its interactions, and an author can see them, or write them by hand. An extension unknown to a tool is dropped. Whether a given tool keeps a subtree of this shape intact, with lists of objects in it, remains to be checked.

### Flow

#### The writing side

- Code attaches one or more active objects to a group with `group::add_active_object()`, or `add_active_object()` of `shapes.group` in Python (see [Changes to `gfx::group`](#changes-to-gfxgroup)).
- During export, the groups pass their declarations to the nodes of the sink, and the exporter gathers them from the whole node tree into one list.
- If the list is empty, nothing more is done.
- The list is validated against the [rules](#rules) of this specification. A violation fails the export, like the other export errors.
- Assimp writes the glTF 2 file into memory: the JSON document of `gltf2`, or the JSON chunk of `glb2`.
- The exporter parses that document, adds the list to the `extras` of the root node as `PYXEL_interactions`, a JSON subtree keeping the order of every ordered map, and writes the file. The same pass adds the lights Assimp cannot write.

#### The reading side

- The importer reads the extra from the glTF document itself, the `.gltf` file or the JSON chunk of a `.glb`, not from the `mMetaData` of an Assimp import (see [Why written after Assimp](#why-written-after-assimp)).
- The `pyxel-io` exporter places `PYXEL_interactions` in the root node, but another application may wrap the model in a new root node, or write a scene with more than one root node. The importer therefore looks for `PYXEL_interactions` in the `extras` of every node of the document.
- A model authored in another tool carries the extra only if that tool writes custom properties as `extras`. Blender's glTF exporter, for example, does so only with **Include → Custom Properties** turned on.
- If the importer finds more than one extra, it merges them into a single list, in the order of their nodes in the `nodes` array of the document. This order decides which object is declared first, for names shared by several objects and for groups moved by several objects.
- It reads each object according to its `version`: as it is, upgraded, without the parts it does not know, or not at all (see [Versions](#versions)). It tells the user about every upgraded object and every dropped one.
- It checks the list against the [rules](#rules) marked for import. A violation is a hard warning, not an error: the importer drops each faulty active object and keeps the rest, and the application must tell the user which objects were dropped and why. For Dollhouse, see [Hard warnings](plans-schema.md#hard-warnings) in the plans schema.
- The geometry is imported without the post-processing that merges or removes named nodes (see [Keeping the named nodes](#keeping-the-named-nodes)).
- It then builds the state machines of the objects, creates a storage template and attaches it to the loaded model.

#### Rules

The exporter fails on a violation of any of these rules. The importer checks those marked for import, and drops the faulty active object: the one breaking the rule, or, for lights sharing a name or described more than once, the active objects of all of them.

| Rule | Import | See |
| ---- | ------ | --- |
| Every active object has a `version` the importer can read: its own major version, or an older one it can upgrade. | yes | [Versions](#versions) |
| Names of active objects, lights and slots contain no `.` and no `/`. | yes | [Names](#names) |
| No two lights of the model share a name, other than the empty one. | yes | [Names](#names) |
| No two active objects describe the same light. | yes | [Names](#names) |
| No group named in a declaration shares its name with a light. | yes | [Names](#names) |
| An entry describing a light has no `slots`, `states` or `actions`. | yes | [Putting it together](#putting-it-together) |
| Slot names are unique within their object, and no slot is named `visible`. | yes | [Slots](#slots), [Geometry-based objects](#geometry-based-objects) |
| Every slot has one of the types defined here; `int` and `color` are reserved. | yes | [Slots](#slots) |
| A declared default value is one the slot's type accepts. | yes | [Slots](#slots) |
| Every group named in `states` or `transitions` is the name of exactly one node of the model, and does not start with `$`. | yes | [Names](#names), [Which groups can move](#which-groups-can-move) |
| Every slot named in a state is a declared output slot or variable. | yes | [Channels](#channels) |
| Every transformation sets a channel its target has, with a value of the channel's type; no list names a channel twice; no state uses `visible`. | yes | [Channels](#channels), [States](#states) |
| The channels of a group keep one order across all states of the object. | yes | [States](#states) |
| Every state named in `when` or `then` is declared, and every action has a `label`. | yes | [Actions](#actions) |
| Every target in `transitions` is one the object can change, and every transition value parses. | yes | [Transitions](#transitions) |
| A model with active objects is exported to `gltf2` or `glb2` only. | not applicable | [File persistence](#file-persistence) |

### Keeping the named nodes

An active object works only as long as the groups it names stay nodes of their own, with their own local matrices: a state transforms a group by multiplying the local matrix of its node (see [Model space](#model-space)). Anything that merges, removes or flattens these nodes breaks the object: the group no longer names exactly one node, the importer drops the object by the [rules](#rules), and the part does not move. Anything that changes the axes of the model space keeps the object, but makes it move the wrong way.

This holds for every step a model goes through after the `pyxel-io` exporter: the importer of a viewer, Assimp post-processing, and other tools that open and save the file, including, but not limited to, Blender. Whether a given tool keeps the nodes, and with which options, is not confirmed by this specification; joining objects, applying transformations or exporting with a flattened hierarchy are the likely ways to lose them.

For an importer using Assimp, the post-processing flags divide as follows.

These break active objects, and an importer must not use them:

- `aiProcess_PreTransformVertices` bakes every transformation into the vertices and flattens the tree: no named node is left, and nothing can move.
- `aiProcess_OptimizeGraph` merges or removes the nodes that no bone, animation, camera or light refers to, named groups included. It may only be used with `AI_CONFIG_PP_OG_EXCLUDE_LIST` naming every group the declarations name, which the importer can collect from `PYXEL_interactions` before the import.
- `aiProcess_MakeLeftHanded` and `aiProcess_ConvertToLeftHanded` mirror the Z axis. The nodes survive, but the model space is no longer the one of [Model space](#model-space): rotations around X and Y turn the other way, and `translate.z` changes its sign. An importer using them must convert the declarations the same way.

These leave the named nodes as they are:

- `aiProcess_Triangulate`, `aiProcess_JoinIdenticalVertices`, `aiProcess_GenNormals`, `aiProcess_GenSmoothNormals`, `aiProcess_CalcTangentSpace`, `aiProcess_SortByPType`, `aiProcess_SplitLargeMeshes`, `aiProcess_ImproveCacheLocality`, `aiProcess_RemoveRedundantMaterials`, `aiProcess_FindDegenerates`, `aiProcess_FindInvalidData`, `aiProcess_GenUVCoords`, `aiProcess_TransformUVCoords`, `aiProcess_FlipUVs`, `aiProcess_ValidateDataStructure`, `aiProcess_GenBoundingBoxes`, `aiProcess_LimitBoneWeights`, `aiProcess_EmbedTextures`.
- `aiProcess_OptimizeMeshes` joins meshes only within one node, so every node keeps its place.
- `aiProcess_FindInstances` replaces identical meshes with references to one of them; the nodes stay.
- `aiProcess_GlobalScale` scales the root node; a translation in the axes of a node below scales with it, so the motion keeps its proportions.

`aiProcess_FlipWindingOrder` on its own touches no node, but it decides, together with a negative scale, which faces are culled (see [Channels](#channels)).

Two more things may happen on import, and neither breaks an object. Assimp may split a glTF node holding a mesh of several primitives into several meshes, or child nodes, but the transformations of a state still apply to the local matrix of the named parent. Assimp may also give a name of its own to an unnamed node; should it collide with the name of a group, the rules catch it, and many 3D viewers can show the hierarchy to the author.

## JSON Schema

[`schemas/interactions.schema.json`](../schemas/interactions.schema.json) checks the `PYXEL_interactions` extra as JSON Schema (draft 2020-12). It checks an object of version 1.0 in full, and an object of any other version only for its `name` and `version`, leaving the rest to the importer (see [Versions](#versions)). The rules that need the whole model, such as which names are groups and which are lights, or which states are declared, are beyond it.

The schema gives what the exporter writes and the importer accepts. Bindings in other languages may be laxer than the JSON: in the [C++ data structures](#c-data-structures), for example, a `light_active_object` may have an empty `light` transition, which the exporter writes as no entry at all, as in the JSON an entry describing a light exists only to declare its transition.

For the same objects in a readable form, see [`docs/examples/interactions-stored.yaml`](examples/interactions-stored.yaml), and [`docs/examples/interactions-loaded.yaml`](examples/interactions-loaded.yaml) for what an importer builds from them.

## API

### C++ data structures

The list as C++23 code would load it, split between two libraries:

- `pyxel-models` holds the declarations, built on its units and on the standard library only, so that a group can keep them (see [Changes to `gfx::group`](#changes-to-gfxgroup));
- `pyxel-io` turns them into JSON and back, next to the exporter that writes the `PYXEL_interactions` extra.

The declarations are not a copy of the JSON:

- each group transformation is a typed alternative, with the missing items of a vector filled with the default value of the channel;
- the keys of a state and of `transitions` are split by their `$` prefix into groups and slots, and the slots are kept under their declared names, so a slot has the same key in both; the `*` of `transitions` is kept apart;
- `when` is always a list;
- `version` is not kept: the declarations are always of the version of this specification. `parse()` brings each object to it, or drops the object, and `serialize()` writes `"1.0"` on every object.

#### The declarations, in `pyxel-models`

```cpp
// <pyxel/gfx/interactions.hpp>

#include <optional>
#include <pyxel/gfx/space.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pyxel::gfx::interactions {
    // angle: radians; length: meters; in memory and in JSON alike

    using state_name = std::string;
    using group_name = std::string;  // a node of the model; no leading "$"
    using slot_name = std::string;   // as declared, without the added "$"

    // ---- group channels ------------------------------------------------

    // One alternative per kind of channel. The vector channels (translate,
    // rotate, scale) stay apart from the single-axis ones, as they are
    // separate channels for the transitions; their missing items are
    // filled with the default value of the channel.
    struct translate_axis {  // translate.x, translate.y, translate.z
        axis on;
        length by;
    };
    struct translate_all {  // translate
        offset by;
    };
    struct rotate_axis {  // rotate.x, rotate.y, rotate.z
        axis around;
        angle by;
    };
    struct rotate_all {  // rotate, applied around X, then Y, then Z
        angle x{}, y{}, z{};
    };
    struct scale_axis {  // scale.x, scale.y, scale.z
        axis along;
        double by{1};
    };
    struct scale_all {  // scale
        double x{1}, y{1}, z{1};
    };

    using group_transformation =
        std::variant<translate_axis, translate_all, rotate_axis, rotate_all, scale_axis, scale_all>;

    // ---- slots ---------------------------------------------------------

    enum class slot_type { floating, factor, boolean, angle, length, offset };  // floating is "float"
    enum class direction { input, output, variable };

    // double for float and factor
    using slot_value = std::variant<bool, double, angle, length, offset>;

    struct slot {
        slot_name name;
        slot_type type;
        interactions::direction direction;
        // "default" in JSON; std::nullopt for the default-initialized
        // value of the type
        std::optional<slot_value> default_value{};
    };

    struct slot_transformation {
        slot_value set;
    };

    // ---- states --------------------------------------------------------

    // One entry of the JSON mapping: the name, and the value split by the
    // "$" prefix of its keys. The members building it are described in
    // Object helper structures.
    struct state {
        state_name name;
        std::unordered_map<group_name, std::vector<group_transformation>> groups{};
        std::unordered_map<slot_name, std::vector<slot_transformation>> slots{};
    };

    // In the order of the JSON, as the first state may be the starting one.
    // A list, as the standard library has no map keeping the order of
    // insertion; an object has a handful of states, so a linear lookup by
    // name is enough.
    using states = std::vector<state>;

    // ---- actions -------------------------------------------------------

    struct action {
        std::string label;
        std::vector<state_name> when;  // a single name is read as a list of one
        state_name then;
        // constructors: see Object helper structures
    };

    // ---- transitions ---------------------------------------------------

    // a comma-separated list of single-channel transitions
    using transition = std::string;

    // the JSON mapping, split like a state: "*" on its own, the other keys
    // by their "$" prefix into groups and slots, the slots kept under
    // their declared names; the members building it are described in
    // Object helper structures
    struct transitions {
        std::optional<transition> defaults{};  // "*" in JSON
        std::unordered_map<group_name, transition> groups{};
        std::unordered_map<slot_name, transition> slots{};
    };

    // ---- active objects ------------------------------------------------

    struct light_active_object {
        std::string name;    // the name of a light in the same model
        transition light{};  // "transitions": {"light": ...} in JSON
    };

    struct geo_active_object {
        std::string name;
        std::vector<slot> slots{};
        interactions::states states{};
        std::vector<action> actions{};
        interactions::transitions transitions{};
    };

    using active_object = std::variant<geo_active_object, light_active_object>;
    using active_objects = std::vector<active_object>;
}  // namespace pyxel::gfx::interactions
```

#### The JSON, in `pyxel-io`

The interface takes and returns the text of the extra, so no JSON library shows in it. The one used inside must keep the keys of a JSON object in the order of the file, for the states: RapidJSON, which `pyxel-io` already uses for the lights, keeps the members of an object in that order, and so does `nlohmann::ordered_json` (but not `nlohmann::json`, which sorts them by name).

```cpp
// <pyxel/io/interactions.hpp>

#include <pyxel/gfx/interactions.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::io::interactions {
    // What parse() did to one active object, other than reading it as is.
    struct note {
        enum kind { upgraded, ignored, dropped };

        std::string object;  // the name of the active object, if it has one
        kind what;
        std::string reason;  // for the user, e.g. "version 2.0 is not supported"
    };

    struct parsed {
        gfx::interactions::active_objects objects;
        std::vector<note> notes;
    };

    // The names of the model's lights tell which entries describe lights.
    // Reads each object on its own, according to its "version" (see
    // Versions). An object it cannot read, being of an unreadable version
    // or not of the shape of the data structures, is left out of `objects`
    // and reported in `notes`; the other objects are still read.
    // Throws std::runtime_error only for text that is not JSON, or JSON that
    // is not an array. The rules of this specification, such as declared
    // states or reserved names, are not checked here: both the exporter and
    // the importer check them, as only they know the whole model.
    parsed parse(std::string_view json, std::span<std::string const> light_names);

    // The text of the PYXEL_interactions extra; the states keep their order.
    std::string serialize(gfx::interactions::active_objects const& objects);
}  // namespace pyxel::io::interactions
```

### Object helper structures

The declarations above are plain data. To build them in code, some of them gain helper members, and `shapes` binds them into Python under the same names (see [The `shapes` module](#the-shapes-module)). A declaration built with the helpers is the same value as one read from the JSON. No helper checks anything that needs the whole object or model: as with `add_active_object()`, those checks wait for the export.

#### `channel`

A set of factories, one per channel, each returning a `group_transformation`. The vector channels take one to three values, the missing ones taking the default value of the channel, as in the JSON.

```cpp
namespace pyxel::gfx::interactions {
    struct channel {
        static translate_axis translate_x(length by) noexcept { return {.on = axis::x, .by = by}; }
        static translate_axis translate_y(length by) noexcept { return {.on = axis::y, .by = by}; }
        static translate_axis translate_z(length by) noexcept { return {.on = axis::z, .by = by}; }
        static translate_all translate(length x, length y = {}, length z = {}) noexcept {
            return {.by = {.x = x, .y = y, .z = z}};
        }
        static translate_all translate(offset const& by) noexcept { return {.by = by}; }

        static rotate_axis rotate_x(angle by) noexcept { return {.around = axis::x, .by = by}; }
        static rotate_axis rotate_y(angle by) noexcept { return {.around = axis::y, .by = by}; }
        static rotate_axis rotate_z(angle by) noexcept { return {.around = axis::z, .by = by}; }
        static rotate_all rotate(angle x, angle y = {}, angle z = {}) noexcept {
            return {.x = x, .y = y, .z = z};
        }

        static scale_axis scale_x(double by) noexcept { return {.along = axis::x, .by = by}; }
        static scale_axis scale_y(double by) noexcept { return {.along = axis::y, .by = by}; }
        static scale_axis scale_z(double by) noexcept { return {.along = axis::z, .by = by}; }
        static scale_all scale(double x, double y = 1, double z = 1) noexcept {
            return {.x = x, .y = y, .z = z};
        }
    };
}  // namespace pyxel::gfx::interactions
```

In Python, `channel` is a class with these static methods. The angles and lengths are the unit values of `shapes`, so `channel.rotate_y(deg(80))` and `channel.translate_x(cm(40))` need no conversion, and `-deg(80)` turns the other way.

#### `state`

A state is built from its name, and each call adds to it and returns the state, so the calls can be chained:

```cpp
namespace pyxel::gfx::interactions {
    struct state {
        // ... name, groups and slots, as declared above

        explicit state(state_name name);

        // Appends the changes to the list of `group`, keeping their order.
        state& transform(group_name const& group, group_transformation const& change);
        state& transform(group_name const& group, std::span<group_transformation const> changes);

        // Sets the output slot or variable `slot`, by its declared name,
        // without the "$" the JSON adds.
        state& set(slot_name const& slot, slot_value const& value);
    };
}  // namespace pyxel::gfx::interactions
```

Usage, for the states of the tilt-and-turn window from the [actions examples](#actions-examples):

```python
from shapes import channel, deg, state

states = [
    state("closed"),
    state("tilted").transform("tilt-hinge", channel.rotate_x(deg(5))),
    state("opened").transform("right-hinge", channel.rotate_y(deg(80))),
    state("opened-wide")
        .transform("right-hinge", channel.rotate_y(deg(80)))
        .transform("left-hinge", channel.rotate_y(-deg(80))),
]
```

`transform` takes a single change or a list of them, and `set` takes a `bool`, a `float`, an angle, a length or an offset, matching the type of the slot.

Due to the rules of [conversions in C++](https://eel.is/c++draft/conv) and their interaction with a `std::variant`, especially one holding `bool` and `double` but no integer type, the binding of `set` must not leave the conversion of the value to pybind11: a Python `int` could become a `bool` instead of a `double`. The binding takes the value as a Python object, checks its Python type, `bool` first, as a Python `bool` is also an `int`, and builds the `slot_value` with an explicit `std::in_place_type_t`: `bool` for a `bool`, `double` for an `int` or a `float`, and the unit types for an angle, a length or an offset. The `default` of `slot` is bound the same way. For the switch from the [transitions examples](#transitions-examples):

```python
state("switched-on").transform("rocker", channel.rotate_x(deg(10))).set("enabled", True)
```

#### `slot`

```cpp
namespace pyxel::gfx::interactions {
    struct slot {
        // ... name, type, direction and default_value, as declared above

        slot(slot_name name, slot_type type, interactions::direction direction,
             std::optional<slot_value> default_value = std::nullopt);
    };
}  // namespace pyxel::gfx::interactions
```

In Python, the enumerations are `slot_type`, with the values `float`, `factor`, `boolean`, `angle`, `length` and `offset`, and `slot_direction`, with `input`, `output` and `variable`. The arguments may be given by keyword, `default` standing for `default_value`:

```python
from shapes import slot, slot_direction, slot_type

slots = [slot(name="enabled", type=slot_type.boolean, direction=slot_direction.output)]
```

#### `action`

```cpp
namespace pyxel::gfx::interactions {
    struct action {
        // ... label, when and then, as declared above

        action(std::string label, std::vector<state_name> when, state_name then);
        action(std::string label, state_name when, state_name then)
            : action(std::move(label), std::vector<state_name>{std::move(when)}, std::move(then)) {}
    };
}  // namespace pyxel::gfx::interactions
```

In Python, `when` takes a single name or a list of them:

```python
from shapes import action

actions = [
    action(label="Tilt", when="closed", then="tilted"),
    action(label="Open one", when="closed", then="opened"),
    # before "Close", so that the default interaction in "opened" opens the second wing
    action(label="Open both", when="opened", then="opened-wide"),
    action(label="Close", when=["tilted", "opened"], then="closed"),
    action(label="Close left", when="opened-wide", then="opened"),
]
```

#### `transitions`

Like a state, the transitions are built call by call, each call returning the transitions, so the calls can be chained. A group and a slot are named by separate calls, so neither needs a prefix: the `$` of a slot reference exists only in the JSON, where the writer adds it and the reader strips it.

```cpp
namespace pyxel::gfx::interactions {
    struct transitions {
        // ... defaults, groups and slots, as declared above

        // The defaults for every target of the object, "*" in the JSON.
        transitions& for_all(transition rule);

        // The transition of a group of the model, by its name.
        transitions& for_group(group_name group, transition rule);

        // The transition of an output slot or a variable, by its declared
        // name, without the "$" the JSON adds.
        transitions& for_slot(slot_name slot, transition rule);
    };
}  // namespace pyxel::gfx::interactions
```

Calling one of them again for the same target replaces the earlier rule. The names avoid `group()`, `slot()` and `default`, which would clash with the members `groups` and `slots` and with a C++ keyword.

Usage, for the switch from the [transitions examples](#transitions-examples):

```python
from shapes import transitions

switch_transitions = (
    transitions()
    .for_group("rocker", "rotate.x 50ms")
    .for_slot("enabled", "0ms linear 45ms")
)
```

and, for the tilt-and-turn window:

```python
window_transitions = (
    transitions()
    .for_all("rotate.x 200ms ease-in, rotate.y 200ms ease-in")
    .for_group("tilt-hinge", "rotate.x 300ms ease-in")
)
```

### Changes to `gfx::group`

The writing side of [File persistence](#file-persistence) attaches active objects to a group through an API this section proposes. A group is the natural place for them: the code building a door, a window or a switch builds its geometry in a group, and can declare how that geometry moves in the same place. A class written once, such as a `sized` subclass in Python, then brings its interactions along wherever it is used.

#### Declaring active objects

`gfx::group` gains a list of active objects, kept in the order they were added:

```cpp
namespace pyxel::gfx {
    struct group : object {
        // ...

        // Keeps a copy of the declaration of a geometry-based object; nothing
        // is checked here, as the groups it names may be added later.
        // Returns the group, so the calls can be chained.
        group& add_active_object(interactions::geo_active_object declaration);

        // The same, with the members of the declaration as arguments, in the
        // order of the structure. The Python binding is this overload, with
        // keyword-only arguments (see The shapes module).
        group& add_active_object(std::string name,
                                 std::vector<interactions::slot> slots = {},
                                 interactions::states states = {},
                                 std::vector<interactions::action> actions = {},
                                 interactions::transitions transitions = {}) {
            return add_active_object(interactions::geo_active_object{
                .name = std::move(name),
                .slots = std::move(slots),
                .states = std::move(states),
                .actions = std::move(actions),
                .transitions = std::move(transitions),
            });
        }

        // Both kinds of objects, in the order they were added; the light
        // ones come from set_light_transition() (see Lights).
        std::vector<interactions::active_object> const& active_objects() const noexcept {
            return active_objects_;
        }

    private:
        // ...
        std::vector<interactions::active_object> active_objects_{};
    };
}  // namespace pyxel::gfx
```

A declaration is a plain value, built from the [C++ data structures](#c-data-structures). It is not checked when added: the groups and lights it names may be added to the tree only later, so the checks wait for the export, when the whole model is known (see [The writing side](#the-writing-side)). The group a declaration is attached to does not limit which groups it may name: any group of the model will do.

#### Which groups can move

Only a named group becomes a node of the exported model (see the comment on `gfx::group`), so only a named group can be named in a state or a transition. An unnamed group has its transformation baked into the vertices, and there is nothing left to move.

The local matrix of a node holds everything since the nearest named group above it: its own transformation and the ones of the unnamed groups in between. A state applies its transformations after that matrix, in the axes and around the origin of the node (see [Model space](#model-space)). A door therefore turns around its hinge only if the named group it moves has its origin on the hinge axis, with the leaf extending away from that origin:

```python
from shapes import action, channel, deg, group, solid, state, transitions

def door(parent: group, width, height, thickness) -> None:
    # the origin of "door-hinge" lies on the hinge axis, and the leaf,
    # a box from (0, 0, 0) to (width, height, thickness), extends along X
    hinge = parent.add_group(name="door-hinge")
    hinge.add(solid.cube(width, height, thickness))

    parent.add_active_object(
        name="door",
        states=[
            state("closed"),
            state("opened").transform(hinge.name, channel.rotate_y(deg(80))),
        ],
        actions=[
            action(label="Open", when="closed", then="opened"),
            action(label="Close", when="opened", then="closed"),
        ],
        transitions=transitions().for_group(hinge.name, "rotate.y 200ms ease-in"),
    )
```

Group names must be unique in the whole model, but the sink never merges siblings of the same name: two nodes named `door-hinge` stay two nodes. A class used more than once in a model, such as the door of a wardrobe with two doors, has to take the names of its groups and active objects from its caller, for example as a prefix. The export fails on a duplicate name that an active object refers to.

#### Lights

A light already keeps its name in `gfx::light`. Nothing changes in `gfx::light` itself. A declaration describing a light holds nothing but its transition, so `add_active_object()`, which takes geometry-based objects only, has no form for it. `gfx::group` gains a separate member instead, called on the group holding the light or on one above it:

```cpp
namespace pyxel::gfx {
    struct group : object {
        // ...

        // Declares the transition of the intensity factor of the light
        // `name`, adding an interactions::light_active_object to the list of
        // active objects, or replacing the transition of the one of the same
        // name already declared on this group. Returns the group, so the
        // calls can be chained.
        group& set_light_transition(std::string name, interactions::transition rule);
    };
}  // namespace pyxel::gfx
```

#### Through the sink

The exporter works on `gfx::sink`, not on the groups, so the declarations travel through it:

- `sink::node` gains a list of active objects, `std::vector<interactions::active_object> active_objects`, and `sink` gains `add_active_object()`, which adds one to the current node.
- `group::output()` adds its declarations after it opens its scope: to its own node for a named group, or to the node of the nearest named group above it otherwise.
- `export_scene()` collects the declarations of every node, in the order of the tree, and runs the steps of [The writing side](#the-writing-side): the checks, the JSON, and the `PYXEL_interactions` extra of the root node. A non-empty list with an export format other than `gltf2` or `glb2` throws `std::runtime_error`.
- A node is kept in the export when an active object names it, even if it has no meshes in it or below, which would otherwise leave it out.

#### Dependencies

`pyxel-models` knows nothing about JSON, and stays that way: a group keeps the declarations from `<pyxel/gfx/interactions.hpp>`, and only the exporter in `pyxel-io` turns them into JSON, with `<pyxel/io/interactions.hpp>` (see [C++ data structures](#c-data-structures)).

#### The `shapes` module

The `shapes` module exposes the same members on `shapes.group`, and binds the helper structures next to it:

- `add_active_object(*, name, slots=[], states=[], actions=[], transitions=transitions())` declares a geometry-based object, as in the door above. `slots`, `states` and `actions` are lists, and `transitions` a single object, all built with the [helper structures](#object-helper-structures), so no name in them carries the `$` of the JSON. The arguments are keyword-only, so a declaration reads like its JSON. There is no `version`: a declaration built in code is always of the version of this specification. Arguments of the wrong type raise `TypeError`; the rules of this specification wait for the export. It returns the group, like the other `add_...` and `set_...` methods.
- `set_light_transition(name, rule)` takes two strings, the name of the light and its transition, such as `"150ms ease-out"`, and returns the group.
- `active_objects` is a read-only property returning copies of the declarations, as `geo_active_object` and `light_active_object` objects. Their attributes follow the C++ structures, the helper structures included.
- `channel`, `state`, `slot`, `slot_type`, `slot_direction`, `action` and `transitions` are the [helper structures](#object-helper-structures). They take the unit values of `shapes`, such as `deg()` and `cm()`, for angles and lengths.

## Viewer support

A viewer, such as Inspector or Dollhouse, does not implement this specification on its own. The pyxel libraries load the interactions and run them; the viewer keeps the clock, draws the result, takes the user's input and connects the models with each other. This section divides the work between the two. It does not define an API: the runtime part (the storage, the ticks, the actions and the signals) has no types or functions yet, and whether it belongs to `pyxel-models`, next to the declarations, to `pyxel-io`, next to the importer, or to a library of its own is left to a later stage. Loading is the only part placed already, in `pyxel-io`.

### Loading

For every glTF 2 model it loads, `pyxel-io` gives the viewer:

- the active objects of the model, read from the `PYXEL_interactions` extras of every node and merged in the order of the nodes (see [The reading side](#the-reading-side)), each read according to its [version](#versions);
- an active object for every named light, with its two input slots and its transition, `none` unless an object describes the light;
- for every geometry-based object, **start**, **actions**, **machine** and **state highlights** (see [Actions](#actions));
- a list of the objects that were dropped, by a [rule](#rules) or by their version, and of those that were upgraded, each with the reason, for the viewer to show to the user (for Dollhouse, see [Hard warnings](plans-schema.md#hard-warnings) and [Warnings](plans-schema.md#warnings));
- a storage template.

A model in any other format has no `PYXEL_interactions` extra to read, and loads with no active objects. Its lights are still exposed as light sources.

### One storage per placement

The libraries create the storage of a placement from the storage template (see [Instantiation](#instantiation)), in the starting state of each object: **start**, unless the viewer chooses another one. The loaded model is shared by all its placements and is never changed by an interaction: everything a placement changes lives in its storage. A viewer therefore draws one model in as many states as it has placements.

### Ticks and actions

The libraries have no clock. The viewer calls them once per tick, with the time elapsed since the previous tick. In that call, the libraries go through the steps of [Order within a tick](#order-within-a-tick): they take the wired signals and the requested actions, evaluate the lights, advance the running transitions and report the changed outputs.

The viewer:

- finds the active object under the pointer, using the state highlights of the current states;
- offers the labels of the available actions as options, skipping the `-1` edge, which has no label;
- binds the default interaction to an input event, such as E, pad A or a double-click;
- asks the libraries to take an action, by its index or by `-1` for the default interaction.

The libraries look the action up in the state machine and start the transition to the state it leads to, interrupting a running one if needed.

### Drawing

For each placement, the libraries give the viewer:

- for each group moved by an active object, the matrix `M` put together from the current values of its channels, composed over all the objects moving the group in the order they are declared (see [States](#states));
- for each named light, the current value of its intensity factor.

While traversing the scene for drawing, the viewer applies `local' = local · M` to each such node (see [Model space](#model-space)), and multiplies the intensity of each light by its factor. Neither changes the loaded model: the matrices and factors apply to this placement only, and only while it is drawn.

### Signals

The libraries:

- report the output slots, variables included, whose value has changed in a tick;
- take the values arriving at input slots, variables included;
- evaluate the update expression of each light source once per tick, after its input slots have received their signals (see [Light sources](#light-sources)).

The wiring belongs to the viewer: in Dollhouse, it comes from the project (see [Wiring objects together](plans-schema.md#wiring-objects-together)). The viewer delivers each reported change along the wires in the next tick, and orders the signals arriving at one input slot in a single tick consistently, as only the last one has lasting consequences (see [Wiring](#wiring)). It may also offer automatic wiring.

The viewer runs the [startup phase](#startup-phase) after placing the models and after restoring a kept storage: it chooses the length of the burst, and calls the libraries for each of its ticks in a mode that makes every transition immediate.

### The kept storage

The libraries:

- extract from the storage of a placement what is kept between runs: for each active object, its name, its kind, its current state, and the name, type and value of each of its slots (see [Keeping the storage between runs](#keeping-the-storage-between-runs));
- restore the storage of a placement from what was kept, following steps 2 to 4 of [Restoring the storage](#restoring-the-storage).

The viewer chooses the placement key, matches the kept storages with the placements (step 1), decides where and how the storage is written, and runs the startup phase once every placement is restored. Two runs showing the same scene at the same time are its concern as well.

### Left to the viewer

- the starting state of an object, if not **start**;
- combining the options and highlights of overlapping active objects, and what the default interaction changes there (see [Actions](#actions));
- the epsilon below which a change duration counts as zero;
- whether the current values of the channels and the change duration are cached;
- the length of the startup burst;
- automatic wiring, if any.
