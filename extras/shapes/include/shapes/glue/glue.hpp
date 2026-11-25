// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pybind11/pybind11.h>
#include <type_traits>

namespace py = pybind11;
using namespace py::literals;
using namespace std::literals;

namespace shapes::glue {
	void bind_angle(py::module_& m);
	void bind_area(py::module_& m);
	void bind_axis(py::module_& m);
	void bind_color(py::module_& m);
	void bind_dmat4(py::module_& m);
	void bind_dmat4_ops(py::module_& m);
	void bind_group(py::module_& m);
	void bind_interactions(py::module_& m);
	void bind_length(py::module_& m);
	void bind_main(py::module_& m);
	void bind_library(py::module_& m);
	void bind_light(py::module_& m);
	void bind_object(py::module_& m);
	void bind_offset(py::module_& m);
	void bind_rng(py::module_& m);
	void bind_sink(py::module_& m);
	void bind_size(py::module_& m);
	void bind_solid(py::module_& m);
	void bind_volume(py::module_& m);
}  // namespace shapes::glue
