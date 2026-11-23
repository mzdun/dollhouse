// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pybind11/critical_section.h>
#include <pybind11/pybind11.h>
#include <utility>

namespace py = pybind11;

// Access to the C++ value of a mutable Python wrapper, safe on a
// free-threaded interpreter. With the GIL, the critical sections do nothing.
//
// A value is copied or changed under the critical section of its wrapper,
// with no Python call in between: a Python call may suspend the section
// (e.g. a garbage collection stopping the world), letting another thread
// change the value halfway. So the arguments are converted before the
// section is entered, and the results are turned into Python objects after
// it is left.
//
// The value must have been given by Python, so that it has a wrapper: the
// lock is taken on that wrapper.
namespace shapes::glue {
	// The result of `read`, called with `value` under the critical section
	// of its wrapper; the result must not refer to the value.
	template <typename T, typename Read>
	auto read(T const& value, Read&& read) {
		auto const wrapper = py::cast(&value, py::return_value_policy::reference);
		py::scoped_critical_section const lock{wrapper};
		return std::forward<Read>(read)(value);
	}

	// A copy of `value`, made under the critical section of its wrapper.
	template <typename T>
	T copy_of(T const& value) {
		return read(value, [](T const& self) { return T{self}; });
	}

	// Calls `change` with `value` under the critical section of its wrapper;
	// returns `value`, so builder methods can return the same wrapper.
	template <typename T, typename Change>
	T& change(T& value, Change&& change) {
		auto const wrapper = py::cast(&value, py::return_value_policy::reference);
		{
			py::scoped_critical_section const lock{wrapper};
			std::forward<Change>(change)(value);
		}
		return value;
	}
}  // namespace shapes::glue
