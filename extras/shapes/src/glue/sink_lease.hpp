// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pybind11/pybind11.h>
#include <cstdint>
#include <pyxel/gfx/sink.hpp>

namespace py = pybind11;

namespace shapes::glue {
	// Lends the sink to a Python `output` call, for the lifetime of the
	// lease. When the lease ends:
	//
	// - the `sink.scope` contexts entered during the call, and still open,
	//   are closed, so an object cannot leave its names and materials to the
	//   objects output after it; while the lease lasts, the scopes entered
	//   during it can be exited only by it, not by the leases nested in it
	//   (e.g. an `output` of a child, called by C++ for this one), nor by
	//   the one it is nested in;
	// - the Python wrapper of the sink is detached (see lifetime.hpp), unless
	//   it existed before the lease, e.g. for an `output` called from
	//   another `output`; a sink kept by Python raises ReferenceError then.
	class sink_lease {
	public:
		using id_type = std::uint64_t;

		explicit sink_lease(pyxel::gfx::sink& out);
		~sink_lease();

		sink_lease(sink_lease const&) = delete;
		sink_lease& operator=(sink_lease const&) = delete;

		py::object const& py_sink() const noexcept { return py_sink_; }

	private:
		pyxel::gfx::sink* out_;
		bool owns_wrapper_;
		py::object py_sink_;
		id_type previous_lease_;
	};
}  // namespace shapes::glue
