// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "names.hpp"
#include <chrono>
#include <cmath>
#include <format>
#include <pyxel/gfx/format.hpp>
#include <string>
#include <type_traits>
#include <variant>

using namespace std::literals;

namespace verify {
	namespace {
		template <class... Ts>
		struct overloaded : Ts... {
			using Ts::operator()...;
		};

		// values computed on the way, such as 1e-15 or -0, print as 0
		double snap_to_zero(double value) noexcept {
			static constexpr double epsilon = 1e-9;
			return std::abs(value) < epsilon ? 0.0 : value;
		}

		std::string format(pyxel::gfx::length value) { return std::format("{}", pyxel::math::dyn(value)); }
		std::string format(pyxel::gfx::angle value) {
			return std::format("{:g}°", snap_to_zero(pyxel::math::degrees{value}.count()));
		}
		std::string format(double value) { return std::format("{:g}", snap_to_zero(value)); }
		std::string format(bool value) { return value ? "true"s : "false"s; }
		std::string format(pyxel::gfx::offset const& value) { return std::format("{}", pyxel::gfx::dyn(value)); }
	}  // namespace

	std::string_view name_of(gfx_ia::easing easing) noexcept {
		switch (easing) {
			case gfx_ia::easing::ease:
				return "ease"sv;
			case gfx_ia::easing::ease_in:
				return "ease-in"sv;
			case gfx_ia::easing::ease_out:
				return "ease-out"sv;
			case gfx_ia::easing::ease_in_out:
				return "ease-in-out"sv;
			case gfx_ia::easing::linear:
				return "linear"sv;
		}
		return "(unknown function)"sv;
	}

	std::string_view name_of(gfx_ia::slot_type type) noexcept {
		switch (type) {
			case gfx_ia::slot_type::factor:
				return "factor"sv;
			case gfx_ia::slot_type::boolean:
				return "boolean"sv;
			case gfx_ia::slot_type::angle:
				return "angle"sv;
			case gfx_ia::slot_type::length:
				return "length"sv;
			case gfx_ia::slot_type::offset:
				return "offset"sv;
			case gfx_ia::slot_type::floating:
				break;
		}
		return "float"sv;
	}

	std::string_view name_of(gfx_ia::direction direction) noexcept {
		switch (direction) {
			case gfx_ia::direction::output:
				return "output"sv;
			case gfx_ia::direction::variable:
				return "variable"sv;
			case gfx_ia::direction::input:
				break;
		}
		return "input"sv;
	}

	std::string_view name_of(gfx_ia::object_kind kind) noexcept {
		switch (kind) {
			case gfx_ia::object_kind::light:
				return "light"sv;
			case gfx_ia::object_kind::geometry:
				break;
		}
		return "geometry"sv;
	}

	std::string_view name_of(pyxel::gfx::light_type type) noexcept {
		switch (type) {
			case pyxel::gfx::light_type::directional:
				return "directional"sv;
			case pyxel::gfx::light_type::spot:
				return "spot"sv;
			case pyxel::gfx::light_type::area:
				return "area"sv;
			case pyxel::gfx::light_type::point:
				break;
		}
		return "point"sv;
	}

	std::string format_seconds(double seconds) {
		// if is under 2 eps from zero, return "0s"s
		if (snap_to_zero(seconds) == 0.0) return "0s"s;
		if (seconds < 1.0) return std::format("{:g}ms", seconds * 1000.0);
		return std::format("{:g}s", seconds);
	}

	std::string format(gfx_ia::timing const& timing) {
		return std::format("{} {} {}", format_seconds(timing.duration), name_of(timing.easing),
		                   format_seconds(timing.delay));
	}

	std::string format(gfx_ia::slot_value const& value) {
		return std::visit([](auto const& alternative) { return format(alternative); }, value);
	}

	std::string format(gfx_ia::group_transformation const& change) {
		auto const value =
		    std::visit(overloaded{
		                   [](gfx_ia::translate_axis const& item) { return format(item.by); },
		                   [](gfx_ia::translate_all const& item) { return format(item.by); },
		                   [](gfx_ia::rotate_axis const& item) { return format(item.by); },
		                   [](gfx_ia::rotate_all const& item) {
			                   return std::format("({}, {}, {})", format(item.x), format(item.y), format(item.z));
		                   },
		                   [](gfx_ia::scale_axis const& item) { return format(item.by); },
		                   [](gfx_ia::scale_all const& item) {
			                   return std::format("({}, {}, {})", format(item.x), format(item.y), format(item.z));
		                   },
		               },
		               change);
		return std::format("{}: {}", gfx_ia::name_of(gfx_ia::channel_of(change)), value);
	}
}  // namespace verify
