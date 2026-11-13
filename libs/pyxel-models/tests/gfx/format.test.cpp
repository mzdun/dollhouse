// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <format>
#include <pyxel/gfx.hpp>
#include <sstream>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	TEST(gfx_format, offset) {
		EXPECT_EQ(std::format("{}", offset{1.0_m, 0.25_m, -2.0_m}), "(1 m, 0.25 m, -2 m)");
		EXPECT_EQ(std::format("[{:^17}]", offset{}), "[ (0 m, 0 m, 0 m) ]");
	}

	TEST(gfx_format, offset_dyn) {
		EXPECT_EQ(std::format("{}", dyn(offset{1.0_m, 15_mm, -5_um})), "(100 cm, 1.5 cm, -0.0005 cm)");
	}

	TEST(gfx_format, size) { EXPECT_EQ(std::format("{}", size{1.0_m, 2.5_m, 0.5_m}), "1 m × 2.5 m × 0.5 m"); }

	TEST(gfx_format, size_dyn) {
		EXPECT_EQ(std::format("{}", dyn(size{1.0_m, 25_cm, 12'345_um})), "100 cm × 25 cm × 1.2345 cm");
	}

	TEST(gfx_format, ostream_matches_format) {
		std::ostringstream os;
		os << offset::at_x(1.0_m) << ' ' << size::cube(2.0_m);
		EXPECT_EQ(os.str(), "(1 m, 0 m, 0 m) 2 m × 2 m × 2 m");
	}
}  // namespace pyxel::gfx::testing
