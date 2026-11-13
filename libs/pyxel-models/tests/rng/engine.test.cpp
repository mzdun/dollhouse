// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <pyxel/rng/engine.hpp>
#include <random>
#include <string>

namespace pyxel::rng::testing {
	namespace {
		std::string const seed_a = "0123456789abcdef0123456789abcdef";
		std::string const seed_b = "fedcba9876543210fedcba9876543210";

		template <size_t Size>
		std::array<engine::result_type, Size> draw(engine& rng) {
			std::array<engine::result_type, Size> result{};
			for (auto& value : result)
				value = rng();
			return result;
		}

		template <size_t Size = 8>
		std::array<engine::result_type, Size> draw(std::string const& seed) {
			engine rng{seed};
			return draw<Size>(rng);
		}

		bool is_lower_hex(std::string const& text) {
			return std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
		}

		void expect_new_seed(std::string const& requested) {
			engine const rng{requested};
			EXPECT_NE(rng.seed(), requested);
			EXPECT_EQ(rng.seed().size(), 32u) << rng.seed();
			EXPECT_TRUE(is_lower_hex(rng.seed())) << rng.seed();
		}
	}  // namespace

	TEST(rng_engine, is_a_uniform_random_bit_generator) {
		static_assert(std::uniform_random_bit_generator<engine>);
		static_assert(engine::min() == 0);
		static_assert(engine::max() == UINT32_MAX);
	}

	TEST(rng_engine, keeps_valid_seed) {
		engine const rng{seed_a};
		EXPECT_EQ(rng.seed(), seed_a);
	}

	TEST(rng_engine, keeps_upper_case_seed) {
		auto const upper = std::string{"0123456789ABCDEF0123456789ABCDEF"};
		engine const rng{upper};
		EXPECT_EQ(rng.seed(), upper);
	}

	TEST(rng_engine, empty_seed_picks_new_hex_seed) {
		engine const first{};
		engine const second{""};
		EXPECT_EQ(first.seed().size(), 32u);
		EXPECT_TRUE(is_lower_hex(first.seed())) << first.seed();
		EXPECT_TRUE(is_lower_hex(second.seed())) << second.seed();
		// 128 random bits; a clash means the seed is not random at all
		EXPECT_NE(first.seed(), second.seed());
	}

	TEST(rng_engine, short_seed_picks_new_seed) { expect_new_seed("abc"); }

	TEST(rng_engine, long_seed_picks_new_seed) { expect_new_seed(seed_a + "0"); }

	TEST(rng_engine, non_hex_seed_picks_new_seed) { expect_new_seed("0123456789abcdef0123456789abcdeg"); }

	TEST(rng_engine, new_seed_can_be_replayed) {
		engine original{};
		engine replay{original.seed()};
		EXPECT_EQ(draw<16>(original), draw<16>(replay));
	}

	TEST(rng_engine, same_seed_same_numbers) { EXPECT_EQ(draw<32>(seed_a), draw<32>(seed_a)); }

	TEST(rng_engine, different_seeds_different_numbers) { EXPECT_NE(draw(seed_a), draw(seed_b)); }

	TEST(rng_engine, seed_case_does_not_matter) { EXPECT_EQ(draw(seed_a), draw("0123456789ABCDEF0123456789ABCDEF")); }

	TEST(rng_engine, numbers_stay_in_range) {
		engine rng{seed_a};
		for (int index = 0; index < 1000; ++index) {
			auto const value = rng();
			EXPECT_GE(value, engine::min());
			EXPECT_LE(value, engine::max());
		}
	}

	TEST(rng_engine, seed_rolls_the_same_numbers_across_versions) {
		// scripts reuse printed seeds; changing these numbers breaks the
		// models built from such seeds; xoshiro128** is fully specified, so
		// these hold for every compiler and standard library
		std::array<engine::result_type, 4> const expected{2576975000, 2576975000, 857404821, 967348376};
		EXPECT_EQ(draw<4>(seed_a), expected);
	}

	TEST(rng_engine, seed_is_the_state) {
		// four 32-bit words, most significant digit first
		engine from_seed{"00000001000000020000000300000004"};
		xoshiro128starstar from_state{{1, 2, 3, 4}};
		for (int index = 0; index < 10; ++index)
			EXPECT_EQ(from_seed(), from_state());
	}

	TEST(rng_engine, zero_seed_still_rolls) {
		engine rng{std::string(32, '0')};
		EXPECT_NE(draw<4>(rng), (std::array<engine::result_type, 4>{}));
	}

	TEST(rng_engine, every_seed_digit_matters) {
		for (size_t index = 0; index < seed_a.size(); ++index) {
			auto changed = seed_a;
			changed[index] = changed[index] == '0' ? '1' : '0';
			EXPECT_NE(draw(seed_a), draw(changed)) << "character " << index << " is ignored";
		}
	}
}  // namespace pyxel::rng::testing
