// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <pyxel/rng/xoshiro.hpp>

namespace pyxel::rng {
	// uniform real "dice", landing anywhere from `min` (included) to `max`
	// (excluded), with a pinned-down algorithm (unlike
	// std::uniform_real_distribution, which differs between the standard
	// libraries), so a seed spins the same numbers on every platform:
	//
	// - a spin takes as many random bits as the mantissa holds (24 for
	//   float, 53 for double), from one 32-bit word of the engine, or from
	//   two (high word first), when one is not enough,
	// - the bits, scaled down to [0, 1), are mapped onto [min, max) with
	//   fma(max - min, fraction, min); a result rounded up to `max` is moved
	//   down to the closest number under it.
	template <std::floating_point Float>
	class basic_spinner {
	public:
		using engine_t = xoshiro128starstar;
		using result_type = Float;

		// the ends are swapped, if `min` is greater than `max`; both ends are
		// expected to be finite
		basic_spinner(engine_t& engine, Float min, Float max) noexcept
		    : engine_{&engine}, min_{(std::min)(min, max)}, max_{(std::max)(min, max)} {}

		Float spin() noexcept {
			auto const fraction = static_cast<Float>(draw()) * scale;
			// always fused, so no compiler is free to round in between
			auto const result = std::fma(max_ - min_, fraction, min_);
			if (result < max_ || min_ == max_) return result;
			return std::nextafter(max_, min_);
		}

		Float min() const noexcept { return min_; }
		Float max() const noexcept { return max_; }

	private:
		static constexpr int bits = std::numeric_limits<Float>::digits;
		static_assert(bits <= 64);
		static_assert(engine_t::min() == 0 && engine_t::max() == UINT32_MAX);

		// 2^-bits
		static constexpr Float scale = Float{1} / static_cast<Float>(std::uint64_t{1} << bits);

		std::uint64_t draw() noexcept {
			std::uint64_t word = (*engine_)();
			if constexpr (bits > 32) word = (word << 32) | (*engine_)();
			auto constexpr drawn = bits > 32 ? 64 : 32;
			return word >> (drawn - bits);
		}

		engine_t* engine_;
		Float min_;
		Float max_;
	};

	using spinner = basic_spinner<double>;
	using spinner_f = basic_spinner<float>;
}  // namespace pyxel::rng
