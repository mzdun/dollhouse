// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "lifetime.hpp"
#include <Python.h>
#include <exception>
#include <format>
#include <string>
#include <string_view>
#include <vector>

// Uses pybind11 internals (pybind11/detail); checked against pybind11 3.0.
// The tests in tests/python/test_ownership.py cover every path through here.

namespace shapes::glue {
	namespace {
		namespace detail = py::detail;

		detail::type_info* registered_type(std::type_info const& type) { return detail::get_type_info(type); }

		// instances registered for the address, holding a `type` value
		std::vector<detail::instance*> instances_of(void const* ptr, detail::type_info const* tinfo) {
			return detail::with_instance_map(ptr, [&](detail::instance_map& instances) {
				std::vector<detail::instance*> result{};
				auto const [first, last] = instances.equal_range(ptr);
				for (auto it = first; it != last; ++it) {
					for (auto* instance_type : detail::all_type_info(Py_TYPE(it->second))) {
						if (instance_type && detail::same_type(*instance_type->cpptype, *tinfo->cpptype)) {
							result.push_back(it->second);
							break;
						}
					}
				}
				return result;
			});
		}

		void detach(detail::instance* inst) {
			// host-owned objects are only ever referenced from Python
			if (inst->owned) return;

			for (auto& v_h : detail::values_and_holders(inst)) {
				if (!v_h || !v_h.holder_constructed()) continue;
				if (v_h.type->holder_enum_v != detail::holder_enum_t::smart_holder) continue;

				if (v_h.instance_registered()) {
					detail::deregister_instance(inst, v_h.value_ptr(), v_h.type);
					v_h.set_instance_registered(false);
				}
				// unowned: drops the pointer, never deletes the object; from
				// now on, the holder reports the instance as disowned
				v_h.holder<py::smart_holder>().release_disowned();
			}
		}

		constexpr std::string_view missing_value = "Missing value for wrapped C++ type `";
		constexpr std::string_view disowned = "`: Python instance was disowned.";

		struct gone_message {
			std::string_view cpp_type;
			std::string_view message;
		};

		constexpr gone_message gone_messages[] = {
		    {"pyxel::gfx::group", "this `group` is gone: the groups of a model live until its model function returns"},
		    {"pyxel::gfx::sink", "this `sink` is gone: a sink lives until the `output` call it was given to returns"},
		    {"pyxel::gfx::sized",
		     "this object is gone: it was added to a group, and the groups of a model live until the model is built"},
		    {"pyxel::gfx::object",
		     "this object is gone: it was added to a group, and the groups of a model live until the model is built"},
		};

		std::string reference_error_message(std::string_view what) {
			auto const cpp_type =
			    what.substr(missing_value.size(), what.size() - missing_value.size() - disowned.size());
			for (auto const& [type, message] : gone_messages) {
				if (type == cpp_type) return std::string{message};
			}
			return std::format("this `{}` is gone", cpp_type);
		}
	}  // namespace

	void detach_wrappers(void const* ptr, std::type_info const& type) noexcept {
		try {
			auto const* tinfo = registered_type(type);
			if (!tinfo) return;
			for (auto* inst : instances_of(ptr, tinfo))
				detach(inst);
		} catch (...) {
			// nothing to report to; a wrapper left attached is no worse than
			// before this function existed
		}
	}

	bool has_wrapper(void const* ptr, std::type_info const& type) {
		auto const* tinfo = registered_type(type);
		return tinfo && !instances_of(ptr, tinfo).empty();
	}

	void bind_lifetime(py::module_&) {
		// pybind11 reports a detached (or disowned) instance with a
		// ValueError naming the C++ type; Python code expects the
		// ReferenceError of a referent, which is gone
		py::register_exception_translator([](std::exception_ptr ptr) {
			if (!ptr) return;
			try {
				std::rethrow_exception(ptr);
			} catch (py::value_error const& err) {
				std::string_view const what = err.what();
				if (what.size() < missing_value.size() + disowned.size() || !what.starts_with(missing_value) ||
				    !what.ends_with(disowned)) {
					throw;
				}
				PyErr_SetString(PyExc_ReferenceError, reference_error_message(what).c_str());
			}
		});
	}
}  // namespace shapes::glue
