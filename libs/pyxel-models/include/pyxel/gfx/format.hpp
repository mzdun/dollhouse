// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <format>
#include <ostream>
#include <pyxel/gfx/space.hpp>
#include <pyxel/math/format.hpp>
#include <string_view>

namespace pyxel::gfx {
	// Formats every component as centimeters with as many decimal places, as needed
	struct dyn_offset_t {
		offset value;
	};

	struct dyn_size_t {
		size value;
	};

	constexpr dyn_offset_t dyn(offset const& v) noexcept { return {v}; }
	constexpr dyn_size_t dyn(size const& v) noexcept { return {v}; }
}  // namespace pyxel::gfx

// (x, y, z)
template <>
struct std::formatter<pyxel::gfx::offset> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::gfx::offset const& value, FormatContext& ctx) const {
		auto const text = std::format("({}, {}, {})", value.x, value.y, value.z);
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

template <>
struct std::formatter<pyxel::gfx::dyn_offset_t> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::gfx::dyn_offset_t const& value, FormatContext& ctx) const {
		using pyxel::math::dyn;
		auto const& [x, y, z] = value.value;
		auto const text = std::format("({}, {}, {})", dyn(x), dyn(y), dyn(z));
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

// width × height × depth
template <>
struct std::formatter<pyxel::gfx::size> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::gfx::size const& value, FormatContext& ctx) const {
		auto const text = std::format("{} × {} × {}", value.width, value.height, value.depth);
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

template <>
struct std::formatter<pyxel::gfx::dyn_size_t> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::gfx::dyn_size_t const& value, FormatContext& ctx) const {
		using pyxel::math::dyn;
		auto const& [width, height, depth] = value.value;
		auto const text = std::format("{} × {} × {}", dyn(width), dyn(height), dyn(depth));
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

namespace pyxel::gfx {
	inline std::ostream& operator<<(std::ostream& out, offset const& value) { return out << std::format("{}", value); }
	inline std::ostream& operator<<(std::ostream& out, size const& value) { return out << std::format("{}", value); }
}  // namespace pyxel::gfx
