// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <cstdint>
#include <pyxel/rng/xoshiro.hpp>
#include <string>

namespace pyxel::rng {
	// uniform integer dice with a pinned-down algorithm (unlike
	// std::uniform_int_distribution, which differs between the standard
	// libraries), so a seed rolls the same numbers on every platform:
	//
	// - the faces are numbered 0 to `span`, then moved by `min`,
	// - each roll takes one 32-bit word from the engine, or two (high word
	//   first), when `span` does not fit in 32 bits,
	// - the bits above the highest bit of `span` are masked off, and the
	//   values over `span` are rejected, so every face has the same chance.
	class dice {
	public:
		using engine_t = xoshiro128starstar;

		// the ends are swapped, if `min` is greater than `max`
		dice(engine_t& engine, std::intmax_t min, std::intmax_t max) noexcept
		    : engine_{&engine}
		    , min_{(std::min)(min, max)}
		    , max_{(std::max)(min, max)}
		    , span_{static_cast<std::uint64_t>(max_) - static_cast<std::uint64_t>(min_)}
		    , mask_{mask_for(span_)} {}

		std::intmax_t roll() noexcept {
			auto value = draw() & mask_;
			while (value > span_)
				value = draw() & mask_;
			// modular, so this is well-defined for any range
			return static_cast<std::intmax_t>(static_cast<std::uint64_t>(min_) + value);
		}

		std::intmax_t min() const noexcept { return min_; }
		std::intmax_t max() const noexcept { return max_; }

	private:
		static_assert(sizeof(std::intmax_t) == sizeof(std::uint64_t));
		static_assert(engine_t::min() == 0 && engine_t::max() == UINT32_MAX);

		static constexpr std::uint64_t mask_for(std::uint64_t span) noexcept {
			span |= span >> 1;
			span |= span >> 2;
			span |= span >> 4;
			span |= span >> 8;
			span |= span >> 16;
			span |= span >> 32;
			return span;
		}

		std::uint64_t draw() noexcept {
			std::uint64_t const high = (*engine_)();
			if (span_ <= UINT32_MAX) return high;
			std::uint64_t const low = (*engine_)();
			return (high << 32) | low;
		}

		engine_t* engine_;
		std::intmax_t min_;
		std::intmax_t max_;
		std::uint64_t span_;  // max - min, as the number of faces may not fit
		std::uint64_t mask_;
	};
}  // namespace pyxel::rng
