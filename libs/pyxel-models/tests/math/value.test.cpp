// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <compare>
#include <concepts>
#include <numbers>
#include <pyxel/math/value.hpp>
#include <type_traits>

namespace pyxel::math::testing {
	template <typename Lhs, typename Rhs>
	concept add_assignable = requires(Lhs lhs, Rhs rhs) { lhs += rhs; };  // NOLINT(readability/braces)

	template <typename Lhs, typename Rhs>
	concept mul_assignable = requires(Lhs lhs, Rhs rhs) { lhs *= rhs; };  // NOLINT(readability/braces)

	TEST(math_value, integer_literals_are_exact_lengths) {
		static_assert(std::is_same_v<decltype(1_m), meters>);
		static_assert(std::is_same_v<decltype(1_dm), decimeters>);
		static_assert(std::is_same_v<decltype(1_cm), centimeters>);
		static_assert(std::is_same_v<decltype(1_mm), millimeters>);
		static_assert(std::is_same_v<decltype(1_um), micrometers>);
		static_assert(std::is_same_v<decltype(1_nm), nanometers>);
		EXPECT_EQ((3_cm).count(), 3);
	}

	TEST(math_value, floating_literals_are_double_lengths) {
		static_assert(std::is_same_v<decltype(1.5_m), meters_f>);
		static_assert(std::is_same_v<decltype(1.5_mm), millimeters_f>);
		static_assert(std::is_same_v<meters_f::storage, double>);
		EXPECT_DOUBLE_EQ((1.5_m).count(), 1.5);
		EXPECT_DOUBLE_EQ((0.1_mm).count(), 0.1);
	}

	TEST(math_value, angle_literals_are_double) {
		static_assert(std::is_same_v<decltype(90_deg), degrees>);
		static_assert(std::is_same_v<decltype(1.5_rad), radians>);
		EXPECT_DOUBLE_EQ((90_deg).count(), 90.0);
	}

	TEST(math_value, default_is_zero) {
		EXPECT_EQ(micrometers{}.count(), 0);
		EXPECT_EQ(meters_f{}.count(), 0.0);
	}

	TEST(math_value, constructs_from_zero) {
		EXPECT_EQ(millimeters{0}.count(), 0);
		EXPECT_EQ(meters_f{0}.count(), 0.0);
	}

	TEST(math_value, integral_storage_rejects_floating_count) {
		static_assert(!std::is_constructible_v<millimeters, double>);
		static_assert(!std::is_constructible_v<millimeters, float>);
		static_assert(std::is_constructible_v<millimeters_f, int>);
		static_assert(!std::is_convertible_v<long long, millimeters>);
	}

	TEST(math_value, lossless_conversions_are_implicit) {
		static_assert(std::is_convertible_v<millimeters, micrometers>);
		static_assert(std::is_convertible_v<meters, nanometers>);
		static_assert(std::is_convertible_v<micrometers, millimeters_f>);
		static_assert(std::is_convertible_v<meters_f, millimeters_f>);

		micrometers const um = 1_mm;
		EXPECT_EQ(um.count(), 1'000);
		EXPECT_EQ(micrometers{2_m}.count(), 2'000'000);
		EXPECT_EQ(millimeters{3_cm}.count(), 30);
		EXPECT_DOUBLE_EQ(millimeters_f{250_um}.count(), 0.25);
	}

	TEST(math_value, lossy_conversions_are_explicit) {
		static_assert(!std::is_convertible_v<micrometers, millimeters>);
		static_assert(!std::is_convertible_v<millimeters_f, millimeters>);
		static_assert(!std::is_convertible_v<meters_f, micrometers>);
		static_assert(std::is_constructible_v<millimeters, micrometers>);
	}

	TEST(math_value, explicit_conversion_truncates) {
		EXPECT_EQ(millimeters{1'500_um}.count(), 1);
		EXPECT_EQ(value_cast<millimeters>(1'999_um).count(), 1);
		EXPECT_EQ(value_cast<millimeters>(-1'500_um).count(), -1);
		EXPECT_EQ(value_cast<millimeters>(millimeters_f{2.75}).count(), 2);
	}

	TEST(math_value, round_to_nearest) {
		EXPECT_EQ(round<millimeters>(1'499_um), 1_mm);
		EXPECT_EQ(round<millimeters>(1'500_um), 2_mm);
		EXPECT_EQ(round<millimeters>(-1'500_um), -2_mm);
		EXPECT_EQ(round<micrometers>(0.0004_mm), 0_um);
		EXPECT_EQ(round<micrometers>(0.0006_mm), 1_um);
	}

	TEST(math_value, value_cast_to_double_keeps_fraction) {
		EXPECT_DOUBLE_EQ(value_cast<meters_f>(1'500_mm).count(), 1.5);
		EXPECT_DOUBLE_EQ(value_cast<meters_f>(25_cm).count(), 0.25);
		EXPECT_EQ(value_cast<micrometers>(meters_f{0.5}).count(), 500'000);
	}

	TEST(math_value, compares_across_units) {
		EXPECT_EQ(1_mm, 1'000_um);
		EXPECT_EQ(1.5_mm, 1'500_um);
		EXPECT_NE(1_mm, 1'001_um);
		EXPECT_LT(999_um, 1_mm);
		EXPECT_GT(1_m, 99_cm);
		EXPECT_LE(2_mm, 2_mm);
		EXPECT_GT(0_mm, -1_mm);
	}

	TEST(math_value, different_quantities_do_not_compare) {
		static_assert(!std::equality_comparable_with<millimeters, area<long long, std::micro>>);
		static_assert(!std::equality_comparable_with<millimeters, radians>);
	}

	TEST(math_value, same_unit_arithmetic) {
		EXPECT_EQ(3_mm + 4_mm, 7_mm);
		EXPECT_EQ(3_mm - 4_mm, -1_mm);
		EXPECT_EQ(-(3_mm), millimeters{-3});

		auto v = 10_mm;
		v += 5_mm;
		EXPECT_EQ(v, 15_mm);
		v -= 20_mm;
		EXPECT_EQ(v, -5_mm);
	}

	TEST(math_value, compound_assignment_accepts_lossless_units) {
		auto v = 1_mm;
		v += 1_cm;
		EXPECT_EQ(v, 11_mm);
		static_assert(add_assignable<micrometers, millimeters>);
		static_assert(!add_assignable<millimeters, micrometers>);
	}

	TEST(math_value, mixed_unit_arithmetic_uses_finer_unit) {
		auto const sum = 1_cm + 1_mm;
		static_assert(std::is_same_v<decltype(sum)::ratio, std::milli>);
		EXPECT_EQ(sum.count(), 11);

		auto const diff = 1_cm - 1_mm;
		static_assert(std::is_same_v<decltype(diff)::ratio, std::milli>);
		EXPECT_EQ(diff.count(), 9);

		EXPECT_EQ((1_m + 1_um).count(), 1'000'001);
	}

	TEST(math_value, mixed_storage_arithmetic_uses_double) {
		auto const sum = 1_mm + 0.5_mm;
		static_assert(std::is_same_v<decltype(sum), millimeters_f const>);
		EXPECT_DOUBLE_EQ(sum.count(), 1.5);
	}

	TEST(math_value, integer_scaling) {
		EXPECT_EQ(3_mm * 2, 6_mm);
		EXPECT_EQ(2 * 3_mm, 6_mm);
		EXPECT_EQ(9_mm / 3, 3_mm);
		EXPECT_EQ(7_mm / 2, 3_mm);
	}

	TEST(math_value, floating_scaling_gives_double) {
		auto const scaled = 3_mm * 1.5;
		static_assert(std::is_same_v<decltype(scaled), millimeters_f const>);
		EXPECT_DOUBLE_EQ(scaled.count(), 4.5);
		EXPECT_DOUBLE_EQ((7_mm / 2.0).count(), 3.5);
		EXPECT_DOUBLE_EQ((meters_f{3.0} / 2).count(), 1.5);
		static_assert(!std::is_convertible_v<decltype(3_mm * 1.5), millimeters>);
	}

	TEST(math_value, compound_scaling) {
		auto v = 3_mm;
		v *= 2;
		EXPECT_EQ(v, 6_mm);
		v /= 4;
		EXPECT_EQ(v, 1_mm);
		static_assert(mul_assignable<millimeters, int>);
		static_assert(!mul_assignable<millimeters, double>);

		auto f = 3.0_mm;
		f *= 1.5;
		EXPECT_DOUBLE_EQ(f.count(), 4.5);
	}

	TEST(math_value, abs) {
		EXPECT_EQ(math::abs(-3_mm), 3_mm);
		EXPECT_EQ((-3_mm).abs(), 3_mm);
		EXPECT_EQ((3_mm).abs(), 3_mm);
		EXPECT_DOUBLE_EQ(meters_f{-1.5}.abs().count(), 1.5);
	}

	TEST(math_value, length_times_length_is_area) {
		auto const product = 2_mm * 3_mm;
		static_assert(std::is_same_v<decltype(product), area<long long, std::micro> const>);
		EXPECT_EQ(product.count(), 6);
		EXPECT_EQ(1_mm * 1_mm, 1'000_um * 1'000_um);
	}

	TEST(math_value, area_times_length_is_volume) {
		auto const product = (2_mm * 3_mm) * 4_mm;
		static_assert(std::is_same_v<decltype(product), volume<long long, std::nano> const>);
		EXPECT_EQ(product.count(), 24);
		EXPECT_EQ(4_mm * (2_mm * 3_mm), product);
		EXPECT_EQ(1_cm * 1_cm * 1_cm, 10_mm * 10_mm * 10_mm);
		static_assert(!std::equality_comparable_with<volume<long long>, area<long long>>);
	}

	TEST(math_value, degrees_convert_to_radians) {
		EXPECT_DOUBLE_EQ(radians{180_deg}.count(), std::numbers::pi);
		EXPECT_DOUBLE_EQ(radians{90_deg}.count(), std::numbers::pi / 2);
		EXPECT_DOUBLE_EQ(radians{-45_deg}.count(), -std::numbers::pi / 4);
		EXPECT_DOUBLE_EQ(radians{360_deg}.count(), 2 * std::numbers::pi);
	}

	TEST(math_value, radians_convert_to_degrees) {
		EXPECT_DOUBLE_EQ(degrees{radians{std::numbers::pi}}.count(), 180.0);
		EXPECT_DOUBLE_EQ(degrees{radians{std::numbers::pi / 2}}.count(), 90.0);
	}

	TEST(math_value, degrees_round_trip) {
		for (auto const deg : {0.0, 1.0, 30.0, 45.0, 90.0, 123.456, 180.0, -270.0, 720.0}) {
			EXPECT_DOUBLE_EQ(degrees{radians{degrees{deg}}}.count(), deg);
		}
	}

	TEST(math_value, mixed_angle_arithmetic_in_radians) {
		auto const sum = 90_deg + radians{std::numbers::pi / 2};
		static_assert(std::is_same_v<decltype(sum), radians const>);
		EXPECT_DOUBLE_EQ(sum.count(), std::numbers::pi);
		EXPECT_EQ(180_deg, radians{std::numbers::pi});
		EXPECT_DOUBLE_EQ((90_deg + 90_deg).count(), 180.0);
	}

	TEST(math_value, trigonometry_accepts_degrees) {
		EXPECT_NEAR(math::sin(90_deg), 1.0, 1e-12);
		EXPECT_NEAR(math::cos(180_deg), -1.0, 1e-12);
		EXPECT_NEAR(math::sin(0_rad), 0.0, 1e-12);
	}
}  // namespace pyxel::math::testing
