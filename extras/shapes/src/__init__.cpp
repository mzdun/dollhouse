// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/pybind11.h>
#include <pyxel/version.hpp>
#include <shapes/glue/glue.hpp>
#include "glue/lifetime.hpp"

namespace py = pybind11;

// The module keeps global state (the libraries, the scopes of the sinks)
// without locks, and relies on the GIL to serialize the calls; a free-threaded
// interpreter turns the GIL on, when importing it, unless the build opts in
// with DOLLHOUSE_FREE_THREADED (see extras/shapes/CMakeLists.txt).
PYBIND11_MODULE(shapes, m, py::mod_gil_not_used(SHAPES_FREE_THREADED != 0)) {
	m.doc() = "Python bindings for building 3D models from scripts";
	m.attr("__version__") = pyxel::version::ui;

	shapes::glue::bind_lifetime(m);

	shapes::glue::bind_rng(m);
	shapes::glue::bind_length(m);
	shapes::glue::bind_area(m);
	shapes::glue::bind_volume(m);
	shapes::glue::bind_angle(m);
	shapes::glue::bind_axis(m);
	shapes::glue::bind_dmat4(m);
	shapes::glue::bind_offset(m);
	shapes::glue::bind_dmat4_ops(m);
	shapes::glue::bind_size(m);
	shapes::glue::bind_color(m);
	shapes::glue::bind_light(m);
	shapes::glue::bind_solid(m);
	shapes::glue::bind_sink(m);
	shapes::glue::bind_object(m);
	shapes::glue::bind_interactions(m);
	shapes::glue::bind_group(m);
	shapes::glue::bind_library(m);
	shapes::glue::bind_main(m);
}
