// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <format>
#include <numbers>
#include <optional>
#include <pyxel/math/format.hpp>
#include <sstream>

namespace pyxel::math::testing {
	TEST(math_format, distance_with_named_unit) {
		EXPECT_EQ(std::format("{}", 3_mm), "3 mm");
		EXPECT_EQ(std::format("{}", 4_cm), "4 cm");
		EXPECT_EQ(std::format("{}", 15_dm), "15 dm");
		EXPECT_EQ(std::format("{}", 5_m), "5 m");
		EXPECT_EQ(std::format("{}", 6_um), "6 um");
		EXPECT_EQ(std::format("{}", 7_nm), "7 nm");
		EXPECT_EQ(std::format("{}", -2_mm), "-2 mm");
	}

	TEST(math_format, floating_distance) {
		EXPECT_EQ(std::format("{}", 1.5_m), "1.5 m");
		EXPECT_EQ(std::format("{}", 2.25_mm), "2.25 mm");
	}

	TEST(math_format, unnamed_unit_in_metres) {
		EXPECT_EQ(std::format("{}", distance<long long, std::ratio<1, 4>>{6}), "1.5 m");
		EXPECT_EQ(std::format("{}", distance<long long, std::kilo>{2}), "2000 m");
	}

	TEST(math_format, uses_string_view_specs) {
		EXPECT_EQ(std::format("[{:>6}]", 3_mm), "[  3 mm]");
		EXPECT_EQ(std::format("[{:*<7}]", 90_deg), "[90°****]");
	}

	TEST(math_format, angle_in_degrees) {
		EXPECT_EQ(std::format("{}", 90_deg), "90°");
		EXPECT_EQ(std::format("{}", radians{std::numbers::pi}), "180°");
		EXPECT_EQ(std::format("{}", -45_deg), "-45°");
	}

	TEST(math_format, dyn_in_centimeters) {
		EXPECT_EQ(std::format("{}", dyn(2_cm)), "2 cm");
		EXPECT_EQ(std::format("{}", dyn(15_mm)), "1.5 cm");
		EXPECT_EQ(std::format("{}", dyn(12'300_um)), "1.23 cm");
		EXPECT_EQ(std::format("{}", dyn(12'340_um)), "1.234 cm");
		EXPECT_EQ(std::format("{}", dyn(12'345_um)), "1.2345 cm");
	}

	TEST(math_format, dyn_keeps_leading_zeros) {
		EXPECT_EQ(std::format("{}", dyn(10'500_um)), "1.05 cm");
		EXPECT_EQ(std::format("{}", dyn(10'050_um)), "1.005 cm");
		EXPECT_EQ(std::format("{}", dyn(5_um)), "0.0005 cm");
	}

	TEST(math_format, dyn_negative) {
		EXPECT_EQ(std::format("{}", dyn(-15_mm)), "-1.5 cm");
		EXPECT_EQ(std::format("{}", dyn(-2_cm)), "-2 cm");
		EXPECT_EQ(std::format("{}", dyn(-5_um)), "-0.0005 cm");
	}

	TEST(math_format, ostream_matches_format) {
		auto const print = [](auto const& d) {
			std::ostringstream os;
			os << d;
			return os.str();
		};
		EXPECT_EQ(print(3_mm), "3 mm");
		EXPECT_EQ(print(15_dm), "15 dm");
		EXPECT_EQ(print(2'000_um), "2000 um");
		EXPECT_EQ(print(1.5_m), "1.5 m");
	}
}  // namespace pyxel::math::testing
