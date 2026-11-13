// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace pyxel::gfx {
	enum class mat_type { paint };

	// sRGB color with linear alpha (0 = transparent, 1 = opaque)
	struct color {
		uint8_t r{};
		uint8_t g{};
		uint8_t b{};
		float alpha{1.0f};

		auto operator<=>(color const&) const = default;
	};

	constexpr color rgb(uint8_t r, uint8_t g, uint8_t b, float alpha = 1.0f) noexcept {
		return {.r = r, .g = g, .b = b, .alpha = alpha};
	}

	struct materials {
		struct paint_opts {
			gfx::color color{};
			float roughness{1.0f};
			float metallic{0.0f};

			auto operator<=>(paint_opts const&) const = default;
		};

		using material_t = std::variant<std::monostate, paint_opts>;

		// Registering the same material twice returns the same name
		std::string register_paint(std::string const& name, paint_opts const& opts);
		std::string register_paint(paint_opts const& opts) { return register_paint({}, opts); }
		// Returns std::monostate for unknown names
		material_t lookup(std::string const&) const;

	private:
		std::string register_material(std::string const& name, material_t const&);

		// Material -> Material.001 -> Material.002 -> ...
		std::string next_name();

		std::map<std::string, material_t> materials_{};
		size_t next_material_{};
	};
}  // namespace pyxel::gfx
