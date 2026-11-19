// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <format>
#include <optional>
#include <ostream>
#include <pyxel/math/value.hpp>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

namespace pyxel::math {
	// Formats a length as centimeters with as many decimal places, as needed
	template <Scalar Storage, typename Ratio>
	struct dyn_t {
		distance<Storage, Ratio> value;
	};

	template <Scalar Storage, typename Ratio>
	constexpr dyn_t<Storage, Ratio> dyn(distance<Storage, Ratio> v) noexcept {
		return {v};
	}

	namespace detail {
		template <typename Ratio>
		constexpr std::string_view length_suffix() noexcept {
			if constexpr (std::ratio_equal_v<Ratio, std::nano>)
				return "nm";
			else if constexpr (std::ratio_equal_v<Ratio, std::micro>)
				return "um";
			else if constexpr (std::ratio_equal_v<Ratio, std::milli>)
				return "mm";
			else if constexpr (std::ratio_equal_v<Ratio, std::centi>)
				return "cm";
			else if constexpr (std::ratio_equal_v<Ratio, std::deci>)
				return "dm";
			else if constexpr (std::ratio_equal_v<Ratio, std::ratio<1>>)
				return "m";
			else
				return {};
		}

		template <Scalar Storage>
		std::string format_count(Storage count) {
			if constexpr (std::floating_point<Storage>)
				return std::format("{:g}", count);
			else
				return std::format("{}", count);
		}

		template <Scalar Storage, typename Ratio>
		inline std::string format_dyn(distance<Storage, Ratio> value) {
			static constexpr unsigned long long um_per_cm = 10'000;
			static constexpr int max_digits = 4;
			// 2^63, the first double too large for long long
			static constexpr double past_long_long = 0x1p63;

			// converting NaN, infinity or anything past long long into
			// micrometers is UB; these fall back to plain centimeters
			auto const exact = micrometers_f{value};
			if (!(std::abs(exact.count()) < past_long_long))
				return std::format("{:g} cm", centimeters_f{exact}.count());

			auto const count = round<micrometers>(exact).count();
			auto const magnitude =
			    count < 0 ? 0ull - static_cast<unsigned long long>(count) : static_cast<unsigned long long>(count);
			std::string_view const sign = count < 0 ? "-" : "";

			auto const whole = magnitude / um_per_cm;
			auto fraction = magnitude % um_per_cm;
			if (fraction == 0) return std::format("{}{} cm", sign, whole);

			auto digits = max_digits;
			while (fraction % 10 == 0) {
				fraction /= 10;
				--digits;
			}
			return std::format("{}{}.{:0{}} cm", sign, whole, fraction, digits);
		}
	}  // namespace detail

	template <Scalar Storage, typename Ratio>
	std::ostream& operator<<(std::ostream& out, distance<Storage, Ratio> const& value) {
		return out << std::format("{}", value);
	}
}  // namespace pyxel::math

template <pyxel::math::Scalar Storage, typename Ratio>
struct std::formatter<pyxel::math::distance<Storage, Ratio>> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::math::distance<Storage, Ratio> const& value, FormatContext& ctx) const {
		using namespace pyxel::math;

		static constexpr auto suffix = detail::length_suffix<Ratio>();
		auto const text = suffix.empty() ? std::format("{} m", detail::format_count(metres_f{value}.count()))
		                                 : std::format("{} {}", detail::format_count(value.count()), suffix);
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

template <pyxel::math::Scalar Storage, typename Ratio>
struct std::formatter<pyxel::math::angle<Storage, Ratio>> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::math::angle<Storage, Ratio> const& value, FormatContext& ctx) const {
		using namespace pyxel::math;

		auto const text = std::format("{}°", detail::format_count(degrees{value}.count()));
		return std::formatter<std::string_view>::format(text, ctx);
	}
};

template <pyxel::math::Scalar Storage, typename Ratio>
struct std::formatter<pyxel::math::dyn_t<Storage, Ratio>> : std::formatter<std::string_view> {
	template <typename FormatContext>
	auto format(pyxel::math::dyn_t<Storage, Ratio> const& value, FormatContext& ctx) const {
		auto const text = pyxel::math::detail::format_dyn(value.value);
		return std::formatter<std::string_view>::format(text, ctx);
	}
};
