// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/math/glm.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include <utility>
#include <vector>
#include "lifetime.hpp"
#include "sink_lease.hpp"

namespace shapes::glue {
	using pyxel::gfx::sink;

	namespace {
		// The scopes entered from Python, innermost last. Python destroys
		// objects whenever the garbage collector gets to them, so the RAII
		// sink::scope_context cannot live in the Python object; it lives
		// here instead, from __enter__ to __exit__, or to the end of the
		// `output` call it was entered in (see sink_lease).
		//
		// Every entry belongs to the lease, i.e. to the `output` call, which
		// entered it; only that call can exit it, so an `output` called by
		// another one can neither close the scopes of its caller, nor leave
		// its own ones behind: they are on top of the caller's, until its
		// lease ends.
		class scope_stack {
		public:
			using id_type = std::uint64_t;

			id_type push(sink& out,
			             std::string const& name,
			             std::string const& material,
			             std::optional<glm::dmat4> const& matrix) {
				// the entry is complete only, if the context did not throw
				auto added = std::make_unique<entry>(last_id_ + 1, current_lease_, out, name, material, matrix);
				auto const id = ++last_id_;
				entries_.push_back(std::move(added));
				return id;
			}

			bool contains(id_type id) const noexcept { return find(id) != nullptr; }

			// true, if the scope was entered in the current `output` call
			bool is_leased_here(id_type id) const noexcept {
				auto const* item = find(id);
				return item && item->lease == current_lease_;
			}

			bool is_innermost(id_type id) const noexcept { return !entries_.empty() && entries_.back()->id == id; }

			void pop() noexcept { entries_.pop_back(); }

			// opens a lease for an `output` call; returns the one it nests in
			sink_lease::id_type begin_lease() noexcept {
				auto const previous = current_lease_;
				current_lease_ = ++last_lease_;
				return previous;
			}

			// closes the scopes the current lease left open, then goes back
			// to the lease it was nested in; the scopes of the outer leases
			// cannot be exited from the inner ones, so the ones of the
			// current lease are all on top
			void end_lease(sink_lease::id_type previous) noexcept {
				while (!entries_.empty() && entries_.back()->lease == current_lease_)
					entries_.pop_back();
				current_lease_ = previous;
			}

		private:
			struct entry {
				entry(id_type id,
				      sink_lease::id_type lease,
				      sink& out,
				      std::string const& name,
				      std::string const& material,
				      std::optional<glm::dmat4> const& matrix)
				    : id{id}, lease{lease}, context{out, name, material, matrix} {}

				id_type id;
				sink_lease::id_type lease;
				sink::scope_context context;
			};

			entry const* find(id_type id) const noexcept {
				auto const it = std::ranges::find(entries_, id, [](auto const& item) { return item->id; });
				return it == entries_.end() ? nullptr : it->get();
			}

			std::vector<std::unique_ptr<entry>> entries_{};
			id_type last_id_{};
			sink_lease::id_type current_lease_{};
			sink_lease::id_type last_lease_{};
		};

		scope_stack& python_scopes() {
			static scope_stack stack{};
			return stack;
		}

		// the Python side of a scope: what to push, and where it is on the
		// stack, while entered
		class python_scope {
		public:
			python_scope(py::object out, std::string name, std::string material, std::optional<glm::dmat4> matrix)
			    : out_{std::move(out)}, name_{std::move(name)}, material_{std::move(material)}, matrix_{matrix} {}

			void enter() {
				auto& scopes = python_scopes();
				if (id_ && scopes.contains(*id_)) throw py::value_error("sink.scope_context: already entered");
				// raises ReferenceError, once the sink is gone
				auto& out = out_.cast<sink&>();
				id_ = scopes.push(out, name_, material_, matrix_);
			}

			void exit() {
				// exited already, possibly by the code it was given to, which
				// then could have entered another scope in its place
				if (!id_) throw py::value_error("sink.scope_context: not entered");
				auto& scopes = python_scopes();
				if (!scopes.contains(*id_)) {
					// already closed at the end of the `output` call
					id_.reset();
					return;
				}
				if (!scopes.is_leased_here(*id_)) {
					throw py::value_error(
					    "sink.scope_context: the scope can only be exited in the `output` call, which entered it");
				}
				if (!scopes.is_innermost(*id_)) {
					throw py::value_error("sink.scope_context: the scopes entered later must be exited first");
				}
				scopes.pop();
				id_.reset();
			}

		private:
			py::object out_;
			std::string name_;
			std::string material_;
			std::optional<glm::dmat4> matrix_;
			std::optional<scope_stack::id_type> id_{};
		};
	}  // namespace

	sink_lease::sink_lease(sink& out)
	    : out_{&out}
	    , owns_wrapper_{!has_wrapper(out)}
	    , py_sink_{py::cast(out, py::return_value_policy::reference)}
	    // last: nothing above may throw with the lease already open
	    , previous_lease_{python_scopes().begin_lease()} {}

	sink_lease::~sink_lease() {
		python_scopes().end_lease(previous_lease_);
		if (owns_wrapper_) detach_wrappers(*out_);
	}

	void bind_sink(py::module_& m) {
		// no constructor and no access to the collected solids; the sink is
		// only provided by the host, to receive the geometry, and only for
		// the duration of an `output` call; the smart holder lets the
		// wrapper be detached afterwards (see lifetime.hpp)
		py::class_<sink, py::smart_holder> cls(
		    m, "sink",
		    "Receives the solids of a model, as a tree of nodes: every named scope (and every named group) "
		    "opens a node, with a transformation of its own, and the solids are kept in the node current when "
		    "they were added, by material. Given to `output`, and only valid until that call returns; a sink "
		    "kept for later raises ReferenceError.");

		py::class_<python_scope>(
		    cls, "scope_context",
		    "Context manager returned by `sink.scope`; opens the node and pushes the material "
		    "on entry, and closes and pops them on exit. Scopes are exited in reverse order of entering, once "
		    "per entry, and only in the `output` call they were entered in; the ones left open are closed when "
		    "that call returns. Exiting a scope, which is not entered, raises ValueError.")
		    .def(
		        "__enter__",
		        [](python_scope& self) -> python_scope& {
			        self.enter();
			        return self;
		        },
		        py::return_value_policy::reference_internal)
		    .def(
		        "__exit__", [](python_scope& self, py::args const&) { self.exit(); },
		        "Pops what was pushed on entry; exceptions are not suppressed.");

		cls.def("add_solid", &sink::add_solid, "solid"_a, "m"_a,
		        "Adds the solid, transformed by the matrix `m`, to the current node, under the current material. "
		        "The matrix is relative to the current node, as is the one given to `output`.")

		    .def("add_light", &sink::add_light, "light"_a, "m"_a,
		         "Adds a copy of the light, placed by the matrix `m` (relative to the current node, as for "
		         "`add_solid`), to the current node; the light shines along the -z of `m`, which must be made of "
		         "a translation, a rotation and a scale only. The material is not used.")

		    .def(
		        "scope",
		        [](py::object const& self, std::string object_name, std::string material,
		           std::optional<glm::dmat4> matrix) {
			        // raises ReferenceError, if the sink is gone already
			        static_cast<void>(self.cast<sink&>());
			        if (matrix && object_name.empty()) {
				        throw py::value_error("sink.scope: a matrix needs an object_name, to open a node for it");
			        }
			        return std::make_unique<python_scope>(self, std::move(object_name), std::move(material), matrix);
		        },
		        "object_name"_a = "", "material"_a = "", "m"_a = py::none(),
		        "Context manager for the `with` statement. A non-empty `object_name` opens a child of the current "
		        "node, transformed by `m` (identity, if None), so the matrices given to `add_solid` inside are "
		        "relative to it; `m` must be made of a translation, a rotation and a scale only (no shear; a "
		        "negative scale is fine). A non-empty `material` is used for the solids inside. `m` without "
		        "an `object_name` raises ValueError. This is the only way to open a node and to change the "
		        "material, so an object can never change the ones set by its parents: an `output` called for a "
		        "child cannot exit the scopes of its caller. Code called directly from an `output` shares its "
		        "scopes, though, as any other part of that call does.");
	}
}  // namespace shapes::glue
