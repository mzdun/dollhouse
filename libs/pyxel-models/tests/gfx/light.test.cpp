// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include <pyxel/gfx.hpp>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	TEST(gfx_light, defaults) {
		light const lamp{};
		EXPECT_EQ(lamp.type, light_type::point);
		EXPECT_EQ(lamp.color, rgb(255, 255, 255));
		EXPECT_FLOAT_EQ(lamp.intensity, 1.0f);
		EXPECT_FALSE(lamp.range);
		// as in KHR_lights_punctual
		EXPECT_EQ(lamp.cone.inner_angle, deg(0));
		EXPECT_EQ(lamp.cone.outer_angle, deg(45));
		EXPECT_EQ(lamp, light{});
	}

	TEST(gfx_light, inverse_square_falloff) {
		light const lamp{};
		EXPECT_FLOAT_EQ(lamp.falloff_at(1_m), 1.0f);
		EXPECT_FLOAT_EQ(lamp.falloff_at(2_m), 0.25f);
		EXPECT_FLOAT_EQ(lamp.falloff_at(50_cm), 4.0f);
		EXPECT_TRUE(std::isinf(lamp.falloff_at(0_m)));
	}

	TEST(gfx_light, range_windows_falloff) {
		light const lamp{.range = 2_m};
		// (1 - (1/2)^4)^2 / 1^2
		EXPECT_FLOAT_EQ(lamp.falloff_at(1_m), (15.0f / 16.0f) * (15.0f / 16.0f));
		EXPECT_FLOAT_EQ(lamp.falloff_at(2_m), 0.0f);
		EXPECT_FLOAT_EQ(lamp.falloff_at(3_m), 0.0f);
	}

	TEST(gfx_light, cone_falloff_is_squared) {
		light::cone_t const cone{.inner_angle = deg(10), .outer_angle = deg(20)};
		EXPECT_FLOAT_EQ(cone.at(deg(0)), 1.0f);
		EXPECT_FLOAT_EQ(cone.at(deg(10)), 1.0f);
		EXPECT_FLOAT_EQ(cone.at(deg(20)), 0.0f);
		EXPECT_FLOAT_EQ(cone.at(deg(90)), 0.0f);

		auto const cos_mid = math::cos(deg(15.0));
		auto const linear = (cos_mid - math::cos(deg(20.0))) / (math::cos(deg(10.0)) - math::cos(deg(20.0)));
		EXPECT_FLOAT_EQ(cone.at(deg(15)), static_cast<float>(linear * linear));
	}

	TEST(gfx_light, cone_hard_edge) {
		light::cone_t const cone{.inner_angle = deg(20), .outer_angle = deg(20)};
		EXPECT_FLOAT_EQ(cone.at(deg(19)), 1.0f);
		EXPECT_FLOAT_EQ(cone.at(deg(21)), 0.0f);
	}

	TEST(gfx_light, illuminance_by_type) {
		light lamp{.intensity = 8.0f};
		// 2 m in front of the light, which shines along -z
		auto const floor = offset::at_z(-2_m);

		// point: 8 cd at 2 m
		EXPECT_FLOAT_EQ(lamp.illuminance_at(floor), 2.0f);

		// directional: the intensity is in lux already
		lamp.type = light_type::directional;
		EXPECT_FLOAT_EQ(lamp.illuminance_at(floor), 8.0f);

		lamp.type = light_type::spot;
		lamp.cone = {.inner_angle = deg(10), .outer_angle = deg(20)};
		EXPECT_FLOAT_EQ(lamp.illuminance_at(floor), 2.0f);
		// 45 deg off the axis
		EXPECT_FLOAT_EQ(lamp.illuminance_at(offset{2_m, 0_m, -2_m}), 0.0f);
	}

	TEST(gfx_light, area_defaults) {
		light::area_t const area{};
		EXPECT_EQ(area.shape, area_shape::square);
		EXPECT_EQ(area.width, 1_m);
		EXPECT_EQ(area.height, 1_m);
		EXPECT_EQ(area.spread, deg(180));
	}

	TEST(gfx_light, area_height_used_by_rectangles_and_ellipses) {
		light::area_t area{.width = 2_m, .height = 3_m};
		EXPECT_EQ(area.effective_height(), 2_m);
		area.shape = area_shape::disk;
		EXPECT_EQ(area.effective_height(), 2_m);
		area.shape = area_shape::rectangle;
		EXPECT_EQ(area.effective_height(), 3_m);
		area.shape = area_shape::ellipse;
		EXPECT_EQ(area.effective_height(), 3_m);
	}

	TEST(gfx_light, area_surface) {
		light::area_t area{.width = 2_m, .height = 3_m};
		EXPECT_DOUBLE_EQ(area.surface().count(), 4.0);
		area.shape = area_shape::rectangle;
		EXPECT_DOUBLE_EQ(area.surface().count(), 6.0);
		area.shape = area_shape::disk;
		EXPECT_DOUBLE_EQ(area.surface().count(), std::numbers::pi);
		area.shape = area_shape::ellipse;
		EXPECT_DOUBLE_EQ(area.surface().count(), 1.5 * std::numbers::pi);
	}

	TEST(gfx_light, shines_along_minus_z) { EXPECT_EQ(light::direction(), (glm::dvec3{0.0, 0.0, -1.0})); }

	TEST(gfx_light, area_emits_within_spread) {
		light lamp{.type = light_type::area, .intensity = 8.0f};
		auto const ahead = offset::at_z(-2_m);
		auto const off_axis = offset{2_m, 0_m, -2_m};

		EXPECT_FLOAT_EQ(lamp.illuminance_at(ahead), 2.0f);
		// 45 deg off the axis, within the default half-space
		EXPECT_GT(lamp.illuminance_at(off_axis), 0.0f);
		// behind the surface
		EXPECT_FLOAT_EQ(lamp.illuminance_at(offset::at_z(2_m)), 0.0f);

		lamp.area.spread = deg(60);
		EXPECT_FLOAT_EQ(lamp.illuminance_at(ahead), 2.0f);
		EXPECT_FLOAT_EQ(lamp.illuminance_at(off_axis), 0.0f);
	}
}  // namespace pyxel::gfx::testing
