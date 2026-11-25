// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cmath>
#include <concepts>
#include <format>
#include <optional>
#include <pyxel/gfx/space.hpp>
#include <shapes/glue/glue.hpp>
#include <stdexcept>
#include <string_view>
#include "length.hpp"

// NaN and infinity have no place in the model: NaN breaks the ordering of
// the vertex maps, and neither can be exported. The numbers coming from the
// scripts are checked here, as Python checks its own:
//
// - a NaN or infinite argument raises ValueError,
// - a division by zero raises ZeroDivisionError,
// - a result, which is too large for a double, raises OverflowError
//   (std::overflow_error, translated by pybind11).
namespace shapes::glue {
	inline bool is_finite(double value) noexcept { return std::isfinite(value); }

	template <typename Value>
	    requires requires(Value const& value) {
		    { value.count() } -> std::convertible_to<double>;
	    }
	bool is_finite(Value const& value) noexcept {
		return std::isfinite(static_cast<double>(value.count()));
	}

	inline bool is_finite(pyxel::gfx::offset const& value) noexcept {
		return is_finite(value.x) && is_finite(value.y) && is_finite(value.z);
	}

	inline bool is_finite(pyxel::gfx::size const& value) noexcept {
		return is_finite(value.width) && is_finite(value.height) && is_finite(value.depth);
	}

	// an argument given by the script; `what` names it, e.g. "cm: value"
	inline double finite(double value, std::string_view what) {
		if (!std::isfinite(value)) throw py::value_error(std::format("{} must be finite, not {}", what, value));
		return value;
	}

	inline std::optional<double> finite(std::optional<double> value, std::string_view what) {
		if (value) finite(*value, what);
		return value;
	}

	// a result of arithmetic on the values of the script
	template <typename Value>
	Value checked(Value const& value, std::string_view type) {
		if (!is_finite(value)) throw std::overflow_error(std::format("{}: the result is too large", type));
		return value;
	}

	[[noreturn]] inline void zero_division(std::string_view type) {
		PyErr_SetString(PyExc_ZeroDivisionError, std::format("{} division by zero", type).c_str());
		throw py::error_already_set();
	}

	// value + value and value - value, and their in-place versions, if asked
	template <typename Value, typename Class>
	void def_checked_sums(Class& cls, char const* type, bool in_place = false) {
		cls.def(
		       "__add__", [type](Value const& self, Value const& rhs) { return checked(self + rhs, type); },
		       py::is_operator())
		    .def(
		        "__sub__", [type](Value const& self, Value const& rhs) { return checked(self - rhs, type); },
		        py::is_operator());
		if (!in_place) return;
		cls.def(
		       "__iadd__",
		       [type](Value& self, Value const& rhs) -> Value& {
			       self = checked(self + rhs, type);
			       return self;
		       },
		       py::is_operator())
		    .def(
		        "__isub__",
		        [type](Value& self, Value const& rhs) -> Value& {
			        self = checked(self - rhs, type);
			        return self;
		        },
		        py::is_operator());
	}

	// value * float, float * value and value / float, and their in-place
	// versions, if asked
	template <typename Value, typename Class>
	void def_checked_scaling(Class& cls, char const* type, bool in_place = false) {
		auto const multiply = [type](Value const& self, double factor) {
			if (!std::isfinite(factor)) finite(factor, std::format("{}: the factor", type));
			return checked(self * factor, type);
		};
		auto const divide = [type](Value const& self, double divisor) {
			if (!std::isfinite(divisor)) finite(divisor, std::format("{}: the divisor", type));
			if (divisor == 0.0) zero_division(type);
			return checked(self / divisor, type);
		};

		cls.def("__mul__", multiply, py::is_operator())
		    .def("__rmul__", multiply, py::is_operator())
		    .def("__truediv__", divide, py::is_operator());
		if (!in_place) return;
		cls.def(
		       "__imul__",
		       [multiply](Value& self, double factor) -> Value& {
			       self = multiply(self, factor);
			       return self;
		       },
		       py::is_operator())
		    .def(
		        "__itruediv__",
		        [divide](Value& self, double divisor) -> Value& {
			        self = divide(self, divisor);
			        return self;
		        },
		        py::is_operator());
	}
}  // namespace shapes::glue
