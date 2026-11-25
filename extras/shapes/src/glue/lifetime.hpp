// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pybind11/pybind11.h>
#include <typeinfo>

namespace py = pybind11;

// Python wrappers of the objects owned by the host (the groups of a model,
// the sink of an export) are plain references; once the host frees the
// object, any Python code still holding the wrapper would read freed memory.
//
// Detaching a wrapper empties it the same way pybind11 empties the objects
// whose ownership went to C++, so using it raises ReferenceError (see
// bind_lifetime) instead. It also forgets the wrapper, so an object created
// later at the same address gets a fresh one. Requires the GIL and the
// `py::smart_holder` for the class.

namespace shapes::glue {
	// detaches the wrappers of the object at `ptr`, registered for `type`;
	// does nothing, if there are none
	void detach_wrappers(void const* ptr, std::type_info const& type) noexcept;

	template <typename T>
	void detach_wrappers(T const& obj) noexcept {
		detach_wrappers(static_cast<void const*>(&obj), typeid(T));
	}

	// true, if a Python wrapper of the object exists already
	bool has_wrapper(void const* ptr, std::type_info const& type);

	template <typename T>
	bool has_wrapper(T const& obj) {
		return has_wrapper(static_cast<void const*>(&obj), typeid(T));
	}

	// registers the translation of the "disowned" errors to ReferenceError
	void bind_lifetime(py::module_& m);
}  // namespace shapes::glue
