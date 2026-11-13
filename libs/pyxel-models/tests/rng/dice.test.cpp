// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <pyxel/rng/engine.hpp>
#include <set>
#include <string>
#include <vector>

namespace pyxel::rng::testing {
	namespace {
		std::string const seed_a = "0123456789abcdef0123456789abcdef";
		std::string const seed_b = "fedcba9876543210fedcba9876543210";

		std::vector<std::intmax_t> roll(dice& die, size_t count) {
			std::vector<std::intmax_t> result{};
			result.reserve(count);
			for (size_t index = 0; index < count; ++index)
				result.push_back(die.roll());
			return result;
		}
	}  // namespace

	// ---- dice ------------------------------------------------------------

	TEST(rng_dice, reports_its_range) {
		engine rng{seed_a};
		auto const die = rng.dice(-3, 7);
		EXPECT_EQ(die.min(), -3);
		EXPECT_EQ(die.max(), 7);
	}

	TEST(rng_dice, rolls_within_range_and_hits_every_face) {
		engine rng{seed_a};
		auto die = rng.dice(-2, 3);
		std::set<std::intmax_t> faces{};
		for (auto const value : roll(die, 1000)) {
			EXPECT_GE(value, -2);
			EXPECT_LE(value, 3);
			faces.insert(value);
		}
		EXPECT_EQ(faces, (std::set<std::intmax_t>{-2, -1, 0, 1, 2, 3}));
	}

	TEST(rng_dice, single_face_always_rolls_it) {
		engine rng{seed_a};
		auto die = rng.dice(5, 5);
		for (auto const value : roll(die, 20))
			EXPECT_EQ(value, 5);
	}

	TEST(rng_dice, rolls_with_the_given_engine) {
		// the dice keeps a pointer, so rolling advances the engine itself;
		// 8 faces never reject, so a roll takes exactly one word
		engine shared{seed_b};
		engine fresh{seed_b};
		auto die = shared.dice(1, 8);
		die.roll();
		fresh();
		for (int index = 0; index < 4; ++index)
			EXPECT_EQ(shared(), fresh());
	}

	TEST(rng_dice, swaps_reversed_ends) {
		engine rng{seed_a};
		engine same{seed_a};
		auto reversed = rng.dice(6, 1);
		auto ordered = same.dice(1, 6);
		EXPECT_EQ(reversed.min(), 1);
		EXPECT_EQ(reversed.max(), 6);
		EXPECT_EQ(roll(reversed, 20), roll(ordered, 20));
	}

	// the pinned rolls below are checked against an independent
	// implementation of the algorithm; changing them breaks the models built
	// from printed seeds

	TEST(rng_dice, rolls_the_same_numbers_across_versions) {
		engine rng{seed_a};
		auto d6 = rng.dice(1, 6);
		EXPECT_EQ(roll(d6, 12), (std::vector<std::intmax_t>{1, 1, 6, 1, 1, 5, 2, 2, 3, 5, 2, 1}));
	}

	TEST(rng_dice, wide_range_takes_two_words_per_roll) {
		engine rng{seed_a};
		auto wide = rng.dice(-1'000'000'000'000, 1'000'000'000'000);
		EXPECT_EQ(roll(wide, 4),
		          (std::vector<std::intmax_t>{-344587996008, 740429103256, 754622471031, -123793628546}));
	}

	TEST(rng_dice, full_range_is_rolled) {
		engine rng{seed_a};
		auto full = rng.dice(INTMAX_MIN, INTMAX_MAX);
		EXPECT_EQ(full.min(), INTMAX_MIN);
		EXPECT_EQ(full.max(), INTMAX_MAX);
		EXPECT_EQ(roll(full, 3),
		          (std::vector<std::intmax_t>{1844651313331799192, -5540846370259693416, 1520901213743623031}));
	}

	TEST(rng_dice, word_sized_range_takes_one_word_per_roll) {
		// 2^32 faces: every word is a face, none is rejected
		engine rng{seed_a};
		engine words{seed_a};
		auto die = rng.dice(0, UINT32_MAX);
		for (int index = 0; index < 10; ++index)
			EXPECT_EQ(die.roll(), static_cast<std::intmax_t>(words()));
	}

	TEST(rng_dice, power_of_two_faces_mask_the_word) {
		// 8 faces: the lowest 3 bits of each word, none is rejected
		engine rng{seed_a};
		engine words{seed_a};
		auto die = rng.dice(0, 7);
		for (int index = 0; index < 10; ++index)
			EXPECT_EQ(die.roll(), static_cast<std::intmax_t>(words() & 7));
	}

	TEST(rng_dice, faces_are_equally_likely) {
		// 60000 rolls of d6: 10000 per face, with sigma close to 91; the
		// limit is over 5 sigma away, while a bias of 1/6 is 1700 away
		engine rng{seed_a};
		auto d6 = rng.dice(1, 6);
		std::array<int, 6> counts{};
		for (int index = 0; index < 60'000; ++index)
			++counts[static_cast<size_t>(d6.roll() - 1)];
		for (auto const count : counts)
			EXPECT_NEAR(count, 10'000, 500);
	}

	// ---- engine -------------------------------------------------------

	TEST(rng_engine_dice, sides_dice_rolls_from_one) {
		engine gen{seed_a};
		auto die = gen.dice(6);
		EXPECT_EQ(die.min(), 1);
		EXPECT_EQ(die.max(), 6);
		for (auto const value : roll(die, 200)) {
			EXPECT_GE(value, 1);
			EXPECT_LE(value, 6);
		}
	}

	TEST(rng_engine_dice, ranged_dice_keeps_range) {
		engine gen{seed_a};
		auto const die = gen.dice(-10, 10);
		EXPECT_EQ(die.min(), -10);
		EXPECT_EQ(die.max(), 10);
	}

	TEST(rng_engine_dice, same_seed_same_rolls) {
		engine first{seed_a};
		engine second{seed_a};
		auto first_die = first.dice(100);
		auto second_die = second.dice(100);
		EXPECT_EQ(roll(first_die, 50), roll(second_die, 50));
	}

	TEST(rng_engine_dice, different_seeds_different_rolls) {
		engine first{seed_a};
		engine second{seed_b};
		auto first_die = first.dice(1000);
		auto second_die = second.dice(1000);
		EXPECT_NE(roll(first_die, 20), roll(second_die, 20));
	}

	TEST(rng_engine_dice, replayed_seed_same_rolls) {
		engine original{};
		engine replay{original.seed()};
		auto original_die = original.dice(20);
		auto replay_die = replay.dice(20);
		EXPECT_EQ(roll(original_die, 50), roll(replay_die, 50));
	}

	TEST(rng_engine_dice, dice_share_the_engine) {
		// two dice of one engine interleave their rolls on its engine,
		// the same as a single dice rolled twice as often
		engine split{seed_a};
		engine single{seed_a};
		auto left = split.dice(6);
		auto right = split.dice(6);
		auto only = single.dice(6);

		std::vector<std::intmax_t> interleaved{};
		for (int index = 0; index < 20; ++index) {
			interleaved.push_back(left.roll());
			interleaved.push_back(right.roll());
		}
		EXPECT_EQ(interleaved, roll(only, 40));
	}
}  // namespace pyxel::rng::testing
