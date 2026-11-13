// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <pyxel/rng/noise.hpp>
#include <string>

namespace pyxel::rng::testing {
	namespace {
		std::string const seed_a = "0123456789abcdef0123456789abcdef";
		std::string const seed_b = "fedcba9876543210fedcba9876543210";

		perlin make_perlin(std::string const& seed) {
			engine rng{seed};
			return perlin{rng};
		}

		octaves make_octaves(std::string const& seed) {
			engine rng{seed};
			return octaves{rng};
		}

		// non-negative only: the lattice is indexed with unsigned coordinates
		struct point {
			float x, y, z;
		};
		constexpr point samples[] = {
		    {0.5f, 0.5f, 0.5f},    {0.25f, 1.75f, 3.125f}, {10.3f, 2.7f, 0.9f},
		    {7.01f, 7.02f, 7.03f}, {123.4f, 56.7f, 8.9f},  {0.1f, 0.2f, 0.3f},
		};
	}  // namespace

	TEST(rng_perlin, zero_at_lattice_nodes) {
		auto const noise = make_perlin(seed_a);
		for (unsigned x = 0; x < 4; ++x) {
			for (unsigned y = 0; y < 4; ++y) {
				for (unsigned z = 0; z < 4; ++z) {
					EXPECT_FLOAT_EQ(noise.noise(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
					                0.0f)
					    << x << ", " << y << ", " << z;
				}
			}
		}
	}

	TEST(rng_perlin, same_seed_same_noise) {
		auto const first = make_perlin(seed_a);
		auto const second = make_perlin(seed_a);
		for (auto const& [x, y, z] : samples)
			EXPECT_EQ(first.noise(x, y, z), second.noise(x, y, z));
	}

	TEST(rng_perlin, different_seeds_different_noise) {
		auto const first = make_perlin(seed_a);
		auto const second = make_perlin(seed_b);
		auto differs = false;
		for (auto const& [x, y, z] : samples)
			differs = differs || first.noise(x, y, z) != second.noise(x, y, z);
		EXPECT_TRUE(differs);
	}

	TEST(rng_perlin, stays_bounded) {
		// unit gradients dotted with offsets from the corners of a unit cube
		auto const noise = make_perlin(seed_a);
		auto const limit = std::sqrt(3.0f);
		for (int x = 0; x < 20; ++x) {
			for (int y = 0; y < 20; ++y) {
				auto const value = noise.noise(static_cast<float>(x) * 0.37f, static_cast<float>(y) * 0.53f,
				                               static_cast<float>(x + y) * 0.29f);
				EXPECT_TRUE(std::isfinite(value));
				EXPECT_LE(std::abs(value), limit);
			}
		}
	}

	TEST(rng_perlin, is_continuous) {
		auto const noise = make_perlin(seed_a);
		for (auto const& [x, y, z] : samples)
			EXPECT_NEAR(noise.noise(x, y, z), noise.noise(x + 1e-3f, y, z), 1e-2f);
	}

	TEST(rng_perlin, is_not_flat) {
		auto const noise = make_perlin(seed_a);
		auto nonzero = false;
		for (auto const& [x, y, z] : samples)
			nonzero = nonzero || std::abs(noise.noise(x, y, z)) > 1e-4f;
		EXPECT_TRUE(nonzero);
	}

	TEST(rng_perlin, draws_one_word_per_gradient_axis_and_shuffle) {
		// 256 gradients of 3 float spins and 256 rolls of a 256-faced dice,
		// each taking exactly one word of the engine
		engine rng{seed_a};
		perlin const noise{rng};
		engine words{seed_a};
		for (int index = 0; index < 256 * 3 + 256; ++index)
			words();
		for (int index = 0; index < 4; ++index)
			EXPECT_EQ(rng(), words());
	}

	TEST(rng_perlin, noise_is_the_same_across_versions) {
		// checked against an independent implementation, rounding each
		// operation to float; changing these breaks the models built from
		// printed seeds
		auto const noise = make_perlin(seed_a);
		std::array<float, std::size(samples)> const expected{
		    -0.20968097448349f,     0.06888677924871445f,  0.19871506094932556f,
		    -0.023619966581463814f, -0.20281991362571716f, -0.05275117605924606f,
		};
		for (size_t index = 0; index < std::size(samples); ++index) {
			auto const& [x, y, z] = samples[index];
			EXPECT_FLOAT_EQ(noise.noise(x, y, z), expected[index]) << x << ", " << y << ", " << z;
		}
	}

	// ---- negative and far coordinates ------------------------------------

	TEST(rng_perlin, zero_at_negative_lattice_nodes) {
		auto const noise = make_perlin(seed_a);
		for (int x = -3; x < 1; ++x) {
			for (int y = -3; y < 1; ++y) {
				for (int z = -3; z < 1; ++z) {
					EXPECT_FLOAT_EQ(noise.noise(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)),
					                0.0f)
					    << x << ", " << y << ", " << z;
				}
			}
		}
	}

	TEST(rng_perlin, repeats_every_256_units) {
		// fractions with few binary digits survive the shift by 256 exactly,
		// so the offsets within the cells are the same on both sides (10.3f
		// would not: it loses low bits, when moved to -245.7f)
		constexpr point exact[] = {
		    {0.5f, 0.5f, 0.5f},     {0.25f, 1.75f, 3.125f},   {10.375f, 2.625f, 0.875f},
		    {7.0625f, 7.5f, 7.25f}, {123.5f, 56.75f, 8.875f}, {0.125f, 0.25f, 0.3125f},
		};
		auto const noise = make_perlin(seed_a);
		for (auto const& [x, y, z] : exact) {
			auto const here = noise.noise(x, y, z);
			EXPECT_EQ(noise.noise(x - 256, y, z), here) << x << ", " << y << ", " << z;
			EXPECT_EQ(noise.noise(x, y - 512, z), here) << x << ", " << y << ", " << z;
			EXPECT_EQ(noise.noise(x, y, z - 256), here) << x << ", " << y << ", " << z;
			EXPECT_EQ(noise.noise(x - 256, y - 256, z - 256), here) << x << ", " << y << ", " << z;
		}
	}

	TEST(rng_perlin, is_continuous_across_zero) {
		auto const noise = make_perlin(seed_a);
		for (auto const& [x, y, z] : samples) {
			EXPECT_NEAR(noise.noise(-1e-4f, y, z), noise.noise(1e-4f, y, z), 1e-3f) << "x, at " << y << ", " << z;
			EXPECT_NEAR(noise.noise(x, -1e-4f, z), noise.noise(x, 1e-4f, z), 1e-3f) << "y, at " << x << ", " << z;
			EXPECT_NEAR(noise.noise(x, y, -1e-4f), noise.noise(x, y, 1e-4f), 1e-3f) << "z, at " << x << ", " << y;
		}
	}

	TEST(rng_perlin, negative_noise_is_not_flat) {
		auto const noise = make_perlin(seed_a);
		auto nonzero = false;
		for (auto const& [x, y, z] : samples)
			nonzero = nonzero || std::abs(noise.noise(-x, -y, -z)) > 1e-4f;
		EXPECT_TRUE(nonzero);
	}

	TEST(rng_perlin, far_coordinates_stay_bounded) {
		// past 2^24 every float is a whole number, so the noise is zero
		// there; before that, it is finite and bounded
		auto const noise = make_perlin(seed_a);
		auto const limit = std::sqrt(3.0f);
		for (auto const v : {-1e9f, -3e5f, -1234.5f, 1234.5f, 3e5f + 0.25f, 1e9f, 5e9f}) {
			auto const value = noise.noise(v, v / 3, -v / 7);
			EXPECT_TRUE(std::isfinite(value)) << v;
			EXPECT_LE(std::abs(value), limit) << v;
		}
		EXPECT_EQ(noise.noise(1e9f, -5e9f, 3e10f), 0.0f);
	}

	TEST(rng_perlin, non_finite_coordinates_give_nan) {
		auto const noise = make_perlin(seed_a);
		auto const inf = std::numeric_limits<float>::infinity();
		auto const nan = std::numeric_limits<float>::quiet_NaN();
		EXPECT_TRUE(std::isnan(noise.noise(inf, 0.5f, 0.5f)));
		EXPECT_TRUE(std::isnan(noise.noise(0.5f, -inf, 0.5f)));
		EXPECT_TRUE(std::isnan(noise.noise(0.5f, 0.5f, nan)));
	}

	TEST(rng_octaves, accept_negative_coordinates) {
		auto const layered = make_octaves(seed_a);
		for (auto const& [x, y, z] : samples) {
			auto const value = layered.noise(-x, -y, -z);
			EXPECT_TRUE(std::isfinite(value));
			EXPECT_LE(std::abs(value), 2 * std::sqrt(3.0f));
		}
	}

	TEST(rng_octaves, single_octave_is_scaled_perlin) {
		auto const source = make_perlin(seed_a);
		auto layered = make_octaves(seed_a);
		layered.set_octaves(1);
		layered.set_frequency(2.0f);
		layered.set_amplitude(3.0f);
		for (auto const& [x, y, z] : samples)
			EXPECT_FLOAT_EQ(layered.noise(x, y, z), 3.0f * source.noise(2 * x, 2 * y, 2 * z));
	}

	TEST(rng_octaves, sums_octaves_with_lacunarity_and_persistence) {
		auto const source = make_perlin(seed_a);
		auto layered = make_octaves(seed_a);
		layered.set_octaves(3);
		layered.set_lacunarity(3.0f);
		layered.set_persistence(0.25f);
		for (auto const& [x, y, z] : samples) {
			auto const expected = source.noise(x, y, z) + 0.25f * source.noise(3 * x, 3 * y, 3 * z) +
			                      0.0625f * source.noise(9 * x, 9 * y, 9 * z);
			EXPECT_NEAR(layered.noise(x, y, z), expected, 1e-6f);
		}
	}

	TEST(rng_octaves, defaults_to_ten_halving_octaves) {
		auto const source = make_perlin(seed_a);
		auto const layered = make_octaves(seed_a);
		for (auto const& [x, y, z] : samples) {
			auto expected = 0.0f;
			auto freq = 1.0f;
			auto amp = 1.0f;
			for (int octave = 0; octave < 10; ++octave) {
				expected += source.noise(x * freq, y * freq, z * freq) * amp;
				freq *= 2.0f;
				amp *= 0.5f;
			}
			EXPECT_FLOAT_EQ(layered.noise(x, y, z), expected);
		}
	}

	TEST(rng_octaves, zero_octaves_are_ignored) {
		auto const reference = make_octaves(seed_a);
		auto layered = make_octaves(seed_a);
		layered.set_octaves(0);
		for (auto const& [x, y, z] : samples)
			EXPECT_EQ(layered.noise(x, y, z), reference.noise(x, y, z));
	}

	TEST(rng_octaves, zero_amplitude_is_silent) {
		auto layered = make_octaves(seed_a);
		layered.set_amplitude(0.0f);
		for (auto const& [x, y, z] : samples)
			EXPECT_EQ(layered.noise(x, y, z), 0.0f);
	}
}  // namespace pyxel::rng::testing
