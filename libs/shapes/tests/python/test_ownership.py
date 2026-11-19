# Copyright (c) 2026 Marcin Zdun
# This code is licensed under MIT license (see LICENSE for details)

"""
Ownership and lifetime across the C++/Python border: Python subclasses
owned by C++ groups, objects handed over with `group.add`, references
outliving the model they came from, and the sink scopes.

The tests which would crash the interpreter, if the bindings regressed, run
in a child interpreter (see `run_python` in conftest.py).
"""

import gc

import pytest
import shapes
from conftest import BuildModel, RunPython, assert_succeeded
from shapes import cm, solid


def cube() -> shapes.solid:
    return solid.cube(cm(1), cm(1), cm(1))


class box(shapes.sized):
    """Outputs a cube nested under `tag`, a Python-only attribute."""

    def __init__(self, tag: str = "box"):
        super().__init__(cm(1), cm(1), cm(1))
        self.tag = tag

    def output(self, sink, m):
        with sink.scope(self.tag):
            sink.add_solid(cube(), m)


# ---------------------------------------------------------------------------
# Python subclasses owned by C++


class TestPythonSubclass:
    def test_override_is_called(self, build_model: BuildModel):
        meshes = build_model(lambda root: root.add(box()))
        assert meshes == {"box": 12}

    def test_temporary_keeps_its_python_part(self, build_model: BuildModel):
        def fill(root: shapes.group):
            # no Python reference survives the call, only the group's
            root.add(box("temporary"))
            gc.collect()

        assert build_model(fill) == {"temporary": 12}

    def test_many_temporaries(self, build_model: BuildModel):
        def fill(root: shapes.group):
            for index in range(50):
                root.add(box(f"b{index}"))
            gc.collect()

        meshes = build_model(fill)
        assert meshes == {f"b{index}": 12 for index in range(50)}

    def test_add_returns_the_same_object(self, build_model: BuildModel):
        seen = {}

        def fill(root: shapes.group):
            child = box()
            returned = root.add(child)
            seen["same"] = returned is child
            # still usable, until the model is built
            returned.scale(2.0)
            seen["size"] = child.size

        build_model(fill)
        assert seen["same"]
        assert seen["size"] == shapes.size(cm(2), cm(2), cm(2))

    def test_subclass_of_subclass(self, build_model: BuildModel):
        class tagged(box):
            def __init__(self):
                super().__init__("derived")

        assert build_model(lambda root: root.add(tagged())) == {"derived": 12}

    def test_missing_output_in_subclass(self, build_model: BuildModel):
        class plain(shapes.sized):
            pass

        with pytest.raises(TypeError, match=r"^plain\.output: not implemented$"):
            build_model(lambda root: root.add(plain(cm(1), cm(1), cm(1))))

    def test_missing_output_in_bare_object(self, build_model: BuildModel):
        with pytest.raises(TypeError, match=r"^object\.output: not implemented$"):
            build_model(lambda root: root.add(shapes.object()))


# ---------------------------------------------------------------------------
# group.add


class TestGroupAdd:
    def test_rejects_other_types(self, build_model: BuildModel):
        with pytest.raises(
            TypeError, match="expected a `shapes.object` or a `shapes.solid`, not int"
        ):
            build_model(lambda root: root.add(42))  # type: ignore[call-overload]

    def test_object_cannot_be_added_twice(self, build_model: BuildModel):
        def fill(root: shapes.group):
            child = box()
            root.add(child)
            root.add(child)

        with pytest.raises(ValueError, match="already belongs to a group"):
            build_model(fill)

    def test_object_cannot_move_to_another_group(self, build_model: BuildModel):
        def fill(root: shapes.group):
            child = box()
            root.add(child)
            root.add_group(name="other").add(child)

        with pytest.raises(ValueError, match="already belongs to a group"):
            build_model(fill)

    def test_group_cannot_be_added(self, build_model: BuildModel):
        def fill(root: shapes.group):
            root.add(root.add_group(name="child"))

        with pytest.raises(ValueError, match="already belongs to a group"):
            build_model(fill)

    def test_root_cannot_be_added(self, build_model: BuildModel):
        def fill(root: shapes.group):
            root.add_group(name="child").add(root)

        with pytest.raises(ValueError, match="already belongs to a group"):
            build_model(fill)

    def test_rejected_object_stays_usable(self, build_model: BuildModel):
        def fill(root: shapes.group):
            child = box("kept")
            root.add_group(name="g").add(child)
            with pytest.raises(ValueError):
                root.add(child)
            child.scale(2.0)  # still owned by `g`, still alive

        assert build_model(fill) == {"g/kept": 12}

    def test_solid_is_copied(self, build_model: BuildModel):
        def fill(root: shapes.group):
            shape = cube()  # 12 triangles
            root.add(shape)
            # a 13th face, after the first copy was taken
            # (not collinear: faces with no area are not exported)
            corners = [
                shape.add_vert(cm(x), cm(y), cm(5)) for x, y in ((0, 0), (1, 0), (0, 1))
            ]
            shape.add_face(corners)
            root.add(shape)

        assert build_model(fill) == {"": 12 + 13}

    def test_child_groups_nest(self, build_model: BuildModel):
        def fill(root: shapes.group):
            outer = root.add_group(name="outer")
            inner = outer.add_group(name="inner")
            del outer
            gc.collect()
            inner.add(box())

        assert build_model(fill) == {"outer/inner/box": 12}


# ---------------------------------------------------------------------------
# References outliving the model
#
# The root group, its children and the sink belong to the host. Groups are
# lent to the model function, and the sink to an `output` call, only for the
# duration of that call. Python code may still hold references to them (e.g.
# stored in a global); using one must raise ReferenceError, not read freed
# memory or crash. These run in a child interpreter, so a regression shows
# up as a failed test, not as a crashed session.

ESCAPED = """
kept = {}

class leaky(shapes.sized):
    def __init__(self):
        super().__init__(cm(1), cm(1), cm(1))

    def output(self, sink, m):
        kept["sink"] = sink
        kept["scope"] = sink.scope("scope")
        sink.add_solid(cube(), m)

@library.model
def escaping(root):
    kept["root"] = root
    kept["child"] = root.add_group(name="child")
    kept["grandchild"] = kept["child"].add_group(name="grandchild")
    kept["object"] = root.add(leaky())
    return "obj"

library.build_models()
gc.collect()

try:
    ACCESS
except ReferenceError as exc:
    print(exc)
"""

GROUP_GONE = (
    "this `group` is gone: the groups of a model live until its model function returns"
)
SINK_GONE = (
    "this `sink` is gone: a sink lives until the `output` call it was given to returns"
)
OBJECT_GONE = (
    "this object is gone: it was added to a group, and the groups of a model live "
    "until the model is built"
)


@pytest.mark.parametrize(
    "access, message",
    [
        pytest.param("kept['object'].scale(2.0)", OBJECT_GONE, id="added-object"),
        pytest.param("kept['root'].add_group(name='late')", GROUP_GONE, id="root"),
        pytest.param("print(kept['root'].name)", GROUP_GONE, id="root-name"),
        pytest.param("kept['root'].add(cube())", GROUP_GONE, id="root-add-solid"),
        pytest.param("kept['child'].set_name('late')", GROUP_GONE, id="child-group"),
        pytest.param("kept['grandchild'].material", GROUP_GONE, id="grandchild-group"),
        pytest.param("kept['sink'].add_solid(cube(), shapes.I)", SINK_GONE, id="sink"),
        pytest.param("kept['sink'].scope('late')", SINK_GONE, id="sink-new-scope"),
        pytest.param("kept['scope'].__enter__()", SINK_GONE, id="sink-scope"),
    ],
)
def test_reference_after_build(run_python: RunPython, access: str, message: str):
    proc = run_python(ESCAPED.replace("ACCESS", access))
    assert_succeeded(proc, message)


def test_group_is_gone_during_export(build_model: BuildModel):
    """Groups are lent to the model function only, not to the `output`
    calls made later, while the model is exported."""
    kept: dict[str, shapes.group] = {}
    errors = []

    class peeking(shapes.sized):
        def __init__(self):
            super().__init__(cm(1), cm(1), cm(1))

        def output(self, sink, m):
            try:
                kept["root"].name
            except ReferenceError as exc:
                errors.append(str(exc))
            sink.add_solid(cube(), m)

    def fill(root: shapes.group):
        kept["root"] = root
        root.add(peeking())

    assert build_model(fill) == {"": 12}
    assert errors == [GROUP_GONE]


def test_sink_is_lent_per_output_call(build_model: BuildModel):
    """A sink kept by one object cannot be used by the next one, even though
    the export is not over yet."""
    kept = {}
    errors = []

    class first(shapes.sized):
        def __init__(self):
            super().__init__(cm(1), cm(1), cm(1))

        def output(self, sink, m):
            kept["sink"] = sink
            sink.add_solid(cube(), m)

    class second(first):
        def output(self, sink, m):
            assert sink is not kept["sink"]
            try:
                kept["sink"].add_solid(cube(), m)
            except ReferenceError as exc:
                errors.append(str(exc))

    def fill(root: shapes.group):
        root.add(first())
        root.add(second())

    assert build_model(fill) == {"": 12}
    assert errors == [SINK_GONE]


def test_each_model_gets_a_new_root(build_model: BuildModel):
    """A later root, possibly at the address of a freed one, never comes
    back as the wrapper of the old root."""
    roots: list[shapes.group] = []
    for _ in range(5):
        build_model(roots.append)
    assert len({id(root) for root in roots}) == len(roots)
    with pytest.raises(ReferenceError):
        _ = roots[0].name


# ---------------------------------------------------------------------------
# sink scopes


def build_with_output(build_model: BuildModel, output) -> dict[str, int]:
    """Builds a model holding one object, which outputs with `output`,
    placed inside the group `outer`."""

    class custom(shapes.sized):
        def __init__(self):
            super().__init__(cm(1), cm(1), cm(1))

        def output(self, sink, m):
            output(sink, m)

    return build_model(lambda root: root.add_group(name="outer").add(custom()))


class TestSinkScope:
    def test_nesting(self, build_model: BuildModel):
        def output(sink, m):
            with sink.scope("a"):
                sink.add_solid(cube(), m)
                with sink.scope("b", "paint"):
                    sink.add_solid(cube(), m)
            sink.add_solid(cube(), m)

        meshes = build_with_output(build_model, output)
        assert meshes == {"outer/a": 12, "outer/a/b": 12, "outer": 12}

    def test_empty_name_adds_no_level(self, build_model: BuildModel):
        def output(sink, m):
            with sink.scope("", "paint"):
                sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer": 12}

    def test_cannot_enter_twice(self, build_model: BuildModel):
        def output(sink, m):
            scope = sink.scope("a")
            with scope:
                with pytest.raises(ValueError, match="already entered"):
                    # pylint: disable-next=unnecessary-dunder-call
                    scope.__enter__()
                sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer/a": 12}

    def test_reentered_after_exit(self, build_model: BuildModel):
        def output(sink, m):
            scope = sink.scope("a")
            with scope:
                sink.add_solid(cube(), m)
            with scope:
                sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer/a": 24}

    def test_exception_pops_the_scope(self, build_model: BuildModel):
        def output(sink, m):
            with pytest.raises(KeyError):
                with sink.scope("failing"):
                    raise KeyError("in scope")
            sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer": 12}

    def test_scope_outlives_its_sink_reference(self, build_model: BuildModel):
        def output(sink, m):
            scope = sink.scope("a")
            add_solid = sink.add_solid
            del sink
            gc.collect()
            with scope:
                add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer/a": 12}

    @pytest.mark.parametrize(
        "name",
        ["push_object_name", "pop_object_name", "push_material", "pop_material"],
    )
    def test_no_raw_stack_access(self, name: str):
        # the name and material stacks are only changed by the scopes, so an
        # object can never pop what its parents pushed
        assert not hasattr(shapes.sink, name)

    def test_exits_in_reverse_order(self, build_model: BuildModel):
        def output(sink, m):
            outer = sink.scope("a")
            inner = sink.scope("b")
            # pylint: disable-next=unnecessary-dunder-call
            outer.__enter__()
            # pylint: disable-next=unnecessary-dunder-call
            inner.__enter__()
            with pytest.raises(ValueError, match="entered later must be exited first"):
                outer.__exit__(None, None, None)
            sink.add_solid(cube(), m)  # both still open
            inner.__exit__(None, None, None)
            outer.__exit__(None, None, None)
            sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer/a/b": 12, "outer": 12}

    def test_exit_without_enter(self, build_model: BuildModel):
        def output(sink, m):
            with pytest.raises(ValueError, match="not entered"):
                sink.scope("a").__exit__(None, None, None)
            sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer": 12}

    def test_exit_twice(self, build_model: BuildModel):
        def output(sink, m):
            with pytest.raises(ValueError, match="not entered"):
                with sink.scope("a") as scope:
                    scope.__exit__(None, None, None)
            sink.add_solid(cube(), m)

        assert build_with_output(build_model, output) == {"outer": 12}

    def test_child_output_cannot_exit_callers_scope(self, build_model: BuildModel):
        """An `output` called by C++ for another one (here, through the base
        class) has a lease of its own: it can neither exit the scopes of its
        caller, nor leave its own ones open for it."""
        errors: list[str] = []
        kept: dict[str, shapes.sink.scope_context] = {}

        class child(shapes.sized):
            def __init__(self):
                super().__init__(cm(1), cm(1), cm(1))

            def output(self, sink, m):
                try:
                    kept["caller"].__exit__(None, None, None)
                except ValueError as exc:
                    errors.append(str(exc))
                # never exited
                # pylint: disable-next=unnecessary-dunder-call
                sink.scope("child", "evil").__enter__()
                sink.add_solid(cube(), m)

        inner = child()

        def output(sink, m):
            with sink.scope("a", "red") as scope:
                kept["caller"] = scope
                shapes.sized.output(inner, sink, m)
                sink.add_solid(cube(), m)
            sink.add_solid(cube(), m)

        meshes = build_with_output(build_model, output)
        assert errors == [
            "sink.scope_context: the scope can only be exited in the `output` "
            "call, which entered it"
        ]
        assert meshes == {"outer/a/child": 12, "outer/a": 12, "outer": 12}

    def test_scope_replaced_by_direct_call_fails_the_model(
        self, build_model: BuildModel
    ):
        """A function called directly from an `output` shares its scopes;
        if it exits a scope it was given and enters another one in its place,
        the `with` of the caller cannot exit the scope it entered, and the
        model fails, instead of being exported with the wrong material."""

        def replace(scope, sink):
            scope.__exit__(None, None, None)
            # pylint: disable-next=unnecessary-dunder-call
            sink.scope("evil", "evil").__enter__()

        def output(sink, m):
            with sink.scope("a", "red") as scope:
                replace(scope, sink)
                sink.add_solid(cube(), m)

        with pytest.raises(ValueError, match="not entered"):
            build_with_output(build_model, output)

    def test_open_scope_closed_after_output(self, build_model: BuildModel):
        class leaving_open(shapes.sized):
            def __init__(self, name: str):
                super().__init__(cm(1), cm(1), cm(1))
                self.name = name

            def output(self, sink, m):
                # never exited
                # pylint: disable-next=unnecessary-dunder-call
                sink.scope(self.name).__enter__()
                sink.add_solid(cube(), m)

        class plain(leaving_open):
            def output(self, sink, m):
                sink.add_solid(cube(), m)

        def fill(root: shapes.group):
            outer = root.add_group(name="outer")
            outer.add(leaving_open("first"))
            outer.add(leaving_open("second"))
            outer.add(plain("third"))
            root.add(plain("sibling"))

        meshes = build_model(fill)
        assert meshes == {"outer/first": 12, "outer/second": 12, "outer": 12, "": 12}

    def test_scope_closed_after_output_can_be_exited(self, build_model: BuildModel):
        kept = {}

        class leaving_open(shapes.sized):
            def __init__(self):
                super().__init__(cm(1), cm(1), cm(1))

            def output(self, sink, m):
                # pylint: disable-next=unnecessary-dunder-call
                kept["scope"] = sink.scope("open").__enter__()
                sink.add_solid(cube(), m)

        build_model(lambda root: root.add(leaving_open()))
        kept["scope"].__exit__(None, None, None)  # already closed: no-op
        with pytest.raises(ReferenceError):
            # its sink is gone
            # pylint: disable-next=unnecessary-dunder-call
            kept["scope"].__enter__()

    def test_exception_in_output_closes_scopes(self, run_python: RunPython):
        """A scope left open by a failing `output` must be closed with the
        failed export, not stay behind for the next one (its sink is gone by
        then)."""
        proc = run_python("""
            failures = []

            class flaky(shapes.sized):
                def output(self, sink, m):
                    sink.scope("flaky").__enter__()
                    if not failures:
                        failures.append("first export")
                        raise KeyError("first export")
                    sink.add_solid(cube(), m)

            class plain(shapes.sized):
                def output(self, sink, m):
                    sink.add_solid(cube(), m)

            @library.model
            def flaky_model(root):
                root.add(flaky(cm(1), cm(1), cm(1)))
                root.add_group(name="g").add(plain(cm(1), cm(1), cm(1)))
                return "obj"

            try:
                library.build_models()
            except KeyError:
                print("failed once")
            gc.collect()
            # the failed model is still registered, and runs again
            library.build_models()
            with open("flaky_model.obj", encoding="UTF-8") as obj:
                for line in obj:
                    if line.startswith("# Mesh"):
                        print(line.strip())
            """)
        assert_succeeded(
            proc,
            "failed once\n# Mesh 'flaky' with 12 faces\n# Mesh 'g' with 12 faces",
        )


# ---------------------------------------------------------------------------
# @library.model


class TestLibraryModel:
    @staticmethod
    def register(name: str, body=None):
        def fn(root: shapes.group):
            return body(root) if body else None

        fn.__name__ = name
        return shapes.library.model(fn)

    def test_needs_one_argument(self):
        def no_root():
            pass

        with pytest.raises(TypeError, match="must be callable with a single argument"):
            shapes.library.model(no_root)  # type: ignore[arg-type]

    def test_returns_the_function(self):
        def fn(_root):
            pass

        assert shapes.library.model(fn) is fn

    def test_rejects_second_registration(self):
        self.register("twice")
        with pytest.raises(ValueError, match="`twice` is already registered"):
            self.register("twice")

    def test_clear_allows_registering_again(self):
        self.register("again")
        shapes.library.clear_models()
        self.register("again")

    def test_clear_forgets_unbuilt_models(self, tmp_path):
        self.register("forgotten", lambda root: root.add(cube()))
        shapes.library.clear_models()
        shapes.library.output_directory = tmp_path
        assert shapes.library.build_models() == []

    def test_rejects_non_str_result(self, tmp_path):
        shapes.library.output_directory = tmp_path
        self.register("numeric", lambda root: 42)
        with pytest.raises(
            TypeError, match=r"must return an exporter id \(str\) or None, not int"
        ):
            shapes.library.build_models()

    def test_rejects_unknown_exporter(self, tmp_path):
        shapes.library.output_directory = tmp_path
        self.register("unknown", lambda root: "no-such-format")
        with pytest.raises(ValueError, match="unknown exporter `no-such-format`"):
            shapes.library.build_models()

    def test_model_exception_passes_through(self, build_model: BuildModel):
        def failing(root: shapes.group):
            raise KeyError("from the model")

        with pytest.raises(KeyError, match="from the model"):
            build_model(failing)

    def test_failed_model_writes_nothing(self, tmp_path):
        shapes.library.output_directory = tmp_path

        def failing(root: shapes.group):
            root.add(cube())
            raise KeyError("from the model")

        self.register("failing", failing)
        with pytest.raises(KeyError):
            shapes.library.build_models()
        assert not list(tmp_path.iterdir())

    def test_failed_model_runs_again(self, tmp_path):
        shapes.library.output_directory = tmp_path
        calls = []

        def flaky(root: shapes.group):
            calls.append(len(calls))
            if len(calls) == 1:
                raise KeyError("first build")
            root.add(cube())
            return "obj"

        self.register("flaky", flaky)
        with pytest.raises(KeyError):
            shapes.library.build_models()
        assert [path.name for path in shapes.library.build_models()] == ["flaky.obj"]
        assert shapes.library.build_models() == []  # built already
        assert calls == [0, 1]

    @pytest.mark.parametrize(
        "call",
        [shapes.library.clear_models, shapes.library.build_models],
        ids=["clear_models", "build_models"],
    )
    def test_library_is_not_reentrant(self, build_model: BuildModel, call):
        with pytest.raises(RuntimeError, match="already building|while building"):
            build_model(lambda root: call())

    def test_root_is_borrowed(self, run_python: RunPython):
        """The host frees the root, while Python still holds the model's
        frame; nothing may be freed twice."""
        proc = run_python("""
            frames = []
            def keeps_frame(root):
                import sys
                frames.append(sys._getframe())
                root.add(cube())
            library.model(keeps_frame)
            library.build_models()
            del frames
            gc.collect()
            print("done")
            """)
        assert_succeeded(proc, "done")
