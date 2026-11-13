// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace pyxel::rng {
	// xoshiro128** 1.1 by David Blackman and Sebastiano Vigna, see
	// https://prng.di.unimi.it/xoshiro128starstar.c; 128 bits of state, 32
	// bits of output, fully specified, so a state rolls the same numbers on
	// every platform
	class xoshiro128starstar {
	public:
		using result_type = std::uint32_t;
		using state_type = std::array<std::uint32_t, 4>;

		// the all-zero state is the one state xoshiro never leaves; it is
		// replaced with the fraction bits of the golden ratio, pi, e and the
		// square root of 2
		static constexpr state_type zero_replacement{0x9e3779b9, 0x243f6a88, 0xb7e15162, 0x6a09e667};

		constexpr explicit xoshiro128starstar(state_type const& state) noexcept
		    : state_{is_zero(state) ? zero_replacement : state} {}

		static constexpr result_type min() noexcept { return 0; }
		static constexpr result_type max() noexcept { return UINT32_MAX; }

		constexpr result_type operator()() noexcept {
			auto& s = state_;
			auto const result = std::rotl(s[1] * 5, 7) * 9;
			auto const t = s[1] << 9;

			s[2] ^= s[0];
			s[3] ^= s[1];
			s[1] ^= s[2];
			s[0] ^= s[3];

			s[2] ^= t;
			s[3] = std::rotl(s[3], 11);

			return result;
		}

		constexpr state_type const& state() const noexcept { return state_; }

	private:
		static constexpr bool is_zero(state_type const& state) noexcept {
			return (state[0] | state[1] | state[2] | state[3]) == 0;
		}

		state_type state_;
	};
}  // namespace pyxel::rng
