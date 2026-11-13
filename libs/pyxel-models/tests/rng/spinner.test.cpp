// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <pyxel/rng/engine.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace pyxel::rng::testing {
	namespace {
		std::string const seed_a = "0123456789abcdef0123456789abcdef";
		std::string const seed_b = "fedcba9876543210fedcba9876543210";

		template <typename Float>
		std::vector<Float> spin(basic_spinner<Float>& spinner, size_t count) {
			std::vector<Float> result{};
			result.reserve(count);
			for (size_t index = 0; index < count; ++index)
				result.push_back(spinner.spin());
			return result;
		}
	}  // namespace

	TEST(rng_spinner, engine_picks_the_precision) {
		engine rng{seed_a};
		static_assert(std::is_same_v<decltype(rng.spinner(0.0, 1.0)), spinner>);
		static_assert(std::is_same_v<decltype(rng.spinner(0.0f, 1.0f)), spinner_f>);
		static_assert(std::is_same_v<decltype(rng.spinner(0.0, 1.0).spin()), double>);
		static_assert(std::is_same_v<decltype(rng.spinner(0.0f, 1.0f).spin()), float>);
	}

	TEST(rng_spinner, reports_its_range) {
		engine rng{seed_a};
		auto const spinner = rng.spinner(-2.5, 4.0);
		EXPECT_EQ(spinner.min(), -2.5);
		EXPECT_EQ(spinner.max(), 4.0);
	}

	TEST(rng_spinner, swaps_reversed_ends) {
		engine rng{seed_a};
		engine same{seed_a};
		auto reversed = rng.spinner(1.0, -1.0);
		auto ordered = same.spinner(-1.0, 1.0);
		EXPECT_EQ(reversed.min(), -1.0);
		EXPECT_EQ(reversed.max(), 1.0);
		EXPECT_EQ(spin(reversed, 20), spin(ordered, 20));
	}

	TEST(rng_spinner, stays_within_half_open_range) {
		engine rng{seed_b};
		auto spinner = rng.spinner(-3.0, 7.0);
		auto spinner_f = rng.spinner(-3.0f, 7.0f);
		for (int index = 0; index < 10'000; ++index) {
			auto const value = spinner.spin();
			EXPECT_GE(value, -3.0);
			EXPECT_LT(value, 7.0);
			auto const value_f = spinner_f.spin();
			EXPECT_GE(value_f, -3.0f);
			EXPECT_LT(value_f, 7.0f);
		}
	}

	TEST(rng_spinner, empty_range_spins_its_end) {
		engine rng{seed_a};
		auto spinner = rng.spinner(2.5, 2.5);
		for (auto const value : spin(spinner, 10))
			EXPECT_EQ(value, 2.5);
	}

	TEST(rng_spinner, rounding_never_reaches_max) {
		// a range one ulp wide: every fraction from one half up would round
		// to max, so all the spins have to land on min
		auto const max = std::nextafter(1.0f, 2.0f);
		engine rng{seed_a};
		auto spinner = rng.spinner(1.0f, max);
		for (auto const value : spin(spinner, 100))
			EXPECT_EQ(value, 1.0f);
	}

	TEST(rng_spinner, float_takes_top_24_bits_of_one_word) {
		engine rng{seed_a};
		engine words{seed_a};
		auto spinner = rng.spinner(0.0f, 1.0f);
		for (int index = 0; index < 10; ++index)
			EXPECT_EQ(spinner.spin(), static_cast<float>(words() >> 8) * 0x1p-24f);
	}

	TEST(rng_spinner, double_takes_top_53_bits_of_two_words) {
		engine rng{seed_a};
		engine words{seed_a};
		auto spinner = rng.spinner(0.0, 1.0);
		for (int index = 0; index < 10; ++index) {
			std::uint64_t const high = words();
			std::uint64_t const low = words();
			EXPECT_EQ(spinner.spin(), static_cast<double>(((high << 32) | low) >> 11) * 0x1p-53);
		}
	}

	// the pinned spins below are checked against an independent
	// implementation of the algorithm; changing them breaks the models built
	// from printed seeds

	TEST(rng_spinner, spins_the_same_numbers_across_versions) {
		engine rng{seed_a};
		auto unit = rng.spinner(0.0, 1.0);
		EXPECT_EQ(spin(unit, 4), (std::vector<double>{0.5999987480696287, 0.19963011639780093, 0.5824482200038338,
		                                              0.22666780930191588}));

		engine shifted_rng{seed_a};
		auto shifted = shifted_rng.spinner(-5.0, 10.0);
		EXPECT_EQ(spin(shifted, 4), (std::vector<double>{3.9999812210444308, -2.005548254032986, 3.736723300057507,
		                                                 -1.5999828604712618}));

		engine float_rng{seed_a};
		auto gradient = float_rng.spinner(-1.0f, 1.0f);
		EXPECT_EQ(spin(gradient, 4), (std::vector<float>{0.1999974250793457f, 0.1999974250793457f, -0.6007398366928101f,
		                                                 -0.5495433807373047f}));
	}

	TEST(rng_spinner, spins_are_evenly_spread) {
		// 100000 spins in 10 buckets: 10000 per bucket, with sigma close to
		// 95; the limit is over 5 sigma away
		engine rng{seed_b};
		auto spinner = rng.spinner(0.0, 10.0);
		std::array<int, 10> counts{};
		for (int index = 0; index < 100'000; ++index)
			++counts[static_cast<size_t>(spinner.spin())];
		for (auto const count : counts)
			EXPECT_NEAR(count, 10'000, 500);
	}

	TEST(rng_spinner, shares_the_engine_with_dice) {
		// a float spin takes one word, like a d8 roll
		engine mixed{seed_b};
		engine words{seed_b};
		auto spinner = mixed.spinner(0.0f, 1.0f);
		auto d8 = mixed.dice(0, 7);
		for (int index = 0; index < 5; ++index) {
			EXPECT_EQ(spinner.spin(), static_cast<float>(words() >> 8) * 0x1p-24f);
			EXPECT_EQ(d8.roll(), static_cast<std::intmax_t>(words() & 7));
		}
	}
}  // namespace pyxel::rng::testing
