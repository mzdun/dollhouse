// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <format>
#include <pyxel/gfx/space.hpp>
#include <string>

namespace shapes::glue {
	inline double to_cm(pyxel::gfx::length value) noexcept { return pyxel::math::centimeters_f{value}.count(); }
	inline double to_mm(pyxel::gfx::length value) noexcept { return pyxel::math::millimeters_f{value}.count(); }

	// 12 significant digits hide the floating-point noise, e.g. 1.7999999999999998
	// becomes 1.8; still a valid Python literal, e.g. 1e-05 for tiny values
	inline std::string number_repr(double value) { return std::format("{:.12g}", value); }

	// evaluates back to the same length in Python, e.g. "cm(1.5)"
	inline std::string length_repr(pyxel::gfx::length value) {
		return std::format("cm({})", number_repr(to_cm(value)));
	}
}  // namespace shapes::glue
