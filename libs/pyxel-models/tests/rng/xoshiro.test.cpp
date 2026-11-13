// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <pyxel/rng/xoshiro.hpp>
#include <random>

namespace pyxel::rng::testing {
	namespace {
		template <size_t Size>
		constexpr std::array<std::uint32_t, Size> draw(xoshiro128starstar::state_type const& state) {
			xoshiro128starstar rng{state};
			std::array<std::uint32_t, Size> result{};
			for (auto& value : result)
				value = rng();
			return result;
		}
	}  // namespace

	TEST(rng_xoshiro, is_a_uniform_random_bit_generator) {
		static_assert(std::uniform_random_bit_generator<xoshiro128starstar>);
		static_assert(xoshiro128starstar::min() == 0);
		static_assert(xoshiro128starstar::max() == UINT32_MAX);
	}

	TEST(rng_xoshiro, matches_reference_vector) {
		// the reference implementation, seeded with {1, 2, 3, 4}
		constexpr std::array<std::uint32_t, 10> expected{
		    11520, 0, 5927040, 70819200, 2031721883, 1637235492, 1287239034, 3734860849, 3729100597, 4258142804,
		};
		static_assert(draw<10>({1, 2, 3, 4}) == expected);
		EXPECT_EQ(draw<10>({1, 2, 3, 4}), expected);
	}

	TEST(rng_xoshiro, keeps_non_zero_state) {
		constexpr xoshiro128starstar::state_type state{0, 0, 0, 1};
		EXPECT_EQ(xoshiro128starstar{state}.state(), state);
	}

	TEST(rng_xoshiro, replaces_zero_state) {
		xoshiro128starstar const rng{{0, 0, 0, 0}};
		EXPECT_EQ(rng.state(), xoshiro128starstar::zero_replacement);
		EXPECT_EQ(draw<3>({0, 0, 0, 0}), (std::array<std::uint32_t, 3>{2463954730, 5524658, 3022325299}));
	}

	TEST(rng_xoshiro, advances_state) {
		xoshiro128starstar rng{{1, 2, 3, 4}};
		auto const before = rng.state();
		rng();
		EXPECT_NE(rng.state(), before);
	}

	TEST(rng_xoshiro, is_a_value) {
		xoshiro128starstar original{{1, 2, 3, 4}};
		original();
		auto copy = original;
		EXPECT_EQ(copy(), original());
		EXPECT_EQ(copy.state(), original.state());
	}
}  // namespace pyxel::rng::testing
