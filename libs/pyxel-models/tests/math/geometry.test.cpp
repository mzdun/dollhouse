// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <pyxel/math/aabb.hpp>
#include <pyxel/math/newell_normal.hpp>
#include <pyxel/math/rotation.hpp>

namespace pyxel::testing {
	template <typename T, glm::qualifier Q>
	::testing::AssertionResult near(glm::vec<3, T, Q> const& actual,
	                                glm::vec<3, T, Q> const& expected,
	                                T eps = T(1e-5)) {
		auto const delta = glm::abs(actual - expected);
		if (delta.x <= eps && delta.y <= eps && delta.z <= eps) return ::testing::AssertionSuccess();
		return ::testing::AssertionFailure() << "(" << actual.x << ", " << actual.y << ", " << actual.z << ") != ("
		                                     << expected.x << ", " << expected.y << ", " << expected.z << ")";
	}

	// ---- AABB ------------------------------------------------------------

	TEST(math_aabb, add_grows_box) {
		AABB box{};
		box.add(1, 2, 3);
		box.add(-1, -2, -3);
		EXPECT_EQ(box.min, glm::vec3(-1, -2, -3));
		EXPECT_EQ(box.max, glm::vec3(1, 2, 3));
	}

	TEST(math_aabb, mid_and_size) {
		AABB const box{.min = {-1, 0, 2}, .max = {3, 4, 4}};
		EXPECT_EQ(box.mid(), glm::vec3(1, 2, 3));
		EXPECT_EQ(box.size(), glm::vec3(4, 4, 2));
	}

	TEST(math_aabb, union_of_boxes) {
		AABB box{.min = {0, 0, 0}, .max = {1, 1, 1}};
		box += AABB{.min = {-2, 0.5f, 0}, .max = {0, 3, 0.5f}};
		EXPECT_EQ(box.min, glm::vec3(-2, 0, 0));
		EXPECT_EQ(box.max, glm::vec3(1, 3, 1));
	}

	TEST(math_aabb, default_box_is_empty) {
		AABB const box{};
		EXPECT_TRUE(box.empty());
		EXPECT_EQ(box.mid(), glm::vec3{});
		EXPECT_EQ(box.size(), glm::vec3{});
	}

	TEST(math_aabb, single_point_is_not_empty) {
		AABB box{};
		box.add(1, 2, 3);
		EXPECT_FALSE(box.empty());
		EXPECT_EQ(box.size(), glm::vec3{});
		EXPECT_EQ(box.mid(), glm::vec3(1, 2, 3));
	}

	TEST(math_aabb, union_with_empty_box) {
		AABB box{.min = {0, 0, 0}, .max = {1, 1, 1}};
		box += AABB{};
		EXPECT_EQ(box.min, glm::vec3(0, 0, 0));
		EXPECT_EQ(box.max, glm::vec3(1, 1, 1));

		AABB empty{};
		empty += box;
		EXPECT_EQ(empty.min, glm::vec3(0, 0, 0));
		EXPECT_EQ(empty.max, glm::vec3(1, 1, 1));
	}

	TEST(math_aabb, points_away_from_origin) {
		AABB box{};
		box.add(1, 1, 1);
		box.add(2, 2, 2);
		EXPECT_EQ(box.min, glm::vec3(1, 1, 1));
		EXPECT_EQ(box.max, glm::vec3(2, 2, 2));
	}

	// ---- rotation --------------------------------------------------------

	TEST(math_rotation, deg_converts_to_radians) {
		EXPECT_DOUBLE_EQ(deg_to_rad(180.0), std::numbers::pi);
		EXPECT_FLOAT_EQ(deg_to_rad(90.0f), std::numbers::pi_v<float> / 2);
		EXPECT_DOUBLE_EQ(deg_to_rad(0.0), 0.0);
	}

	TEST(math_rotation, compass_yaws) {
		EXPECT_DOUBLE_EQ(rotation_d::yaw_south, 0.0);
		EXPECT_DOUBLE_EQ(rotation_d::yaw_east, std::numbers::pi / 2);
		EXPECT_DOUBLE_EQ(rotation_d::yaw_north, std::numbers::pi);
		EXPECT_DOUBLE_EQ(rotation_d::yaw_west, -std::numbers::pi / 2);
	}

	TEST(math_rotation, from_direction) {
		auto const forward = rotation::from({0, 0, 1});
		EXPECT_NEAR(forward.yaw, 0.f, 1e-6f);
		EXPECT_NEAR(forward.pitch, 0.f, 1e-6f);

		auto const east = rotation::from({5, 0, 0});
		EXPECT_NEAR(east.yaw, std::numbers::pi_v<float> / 2, 1e-6f);
		EXPECT_NEAR(east.pitch, 0.f, 1e-6f);

		auto const up = rotation::from({0, 2, 0});
		EXPECT_NEAR(up.pitch, std::numbers::pi_v<float> / 2, 1e-6f);
	}

	TEST(math_rotation, direction_is_unit_vector) {
		EXPECT_TRUE(near(rotation{.yaw = 0, .pitch = 0}.direction(), glm::vec3(0, 0, 1)));
		EXPECT_TRUE(near(rotation{.yaw = deg_to_rad(90.f), .pitch = 0}.direction(), glm::vec3(1, 0, 0)));
		EXPECT_TRUE(near(rotation{.yaw = 0, .pitch = deg_to_rad(90.f)}.direction(), glm::vec3(0, 1, 0)));
		EXPECT_NEAR(glm::length(rotation{.yaw = 1.2f, .pitch = -.4f}.direction()), 1.f, 1e-6f);
	}

	TEST(math_rotation, from_and_direction_round_trip) {
		for (auto const& looking : {glm::vec3(1, 2, 3), glm::vec3(-1, .5f, -2), glm::vec3(0, -1, 4)}) {
			EXPECT_TRUE(near(rotation::from(looking).direction(), glm::normalize(looking)));
		}
	}

	TEST(math_rotation, right_is_horizontal_and_perpendicular) {
		rotation r{.yaw = .7f, .pitch = .3f};
		auto const right = r.right();
		EXPECT_FLOAT_EQ(right.y, 0.f);
		EXPECT_NEAR(glm::dot(right, r.direction()), 0.f, 1e-6f);
		EXPECT_TRUE(near(rotation{}.right(), glm::vec3(-1, 0, 0)));
	}

	TEST(math_rotation, clip_wraps_yaw_and_clamps_pitch) {
		auto const pi = std::numbers::pi_v<double>;

		auto r = rotation_d{.yaw = -pi / 2, .pitch = 2.0};
		r.clip();
		EXPECT_NEAR(r.yaw, 3 * pi / 2, 1e-12);
		EXPECT_DOUBLE_EQ(r.pitch, pi / 2);

		r = rotation_d{.yaw = 5 * pi, .pitch = -2.0};
		r.clip();
		EXPECT_NEAR(r.yaw, pi, 1e-12);
		EXPECT_DOUBLE_EQ(r.pitch, -pi / 2);
	}

	TEST(math_rotation, clip_keeps_yaw_below_full_turn) {
		auto const pi = std::numbers::pi_v<double>;

		auto r = rotation_d{.yaw = 2 * pi};
		r.clip();
		EXPECT_DOUBLE_EQ(r.yaw, 0.0);

		r = rotation_d{.yaw = -1e-20};
		r.clip();
		EXPECT_GE(r.yaw, 0.0);
		EXPECT_LT(r.yaw, 2 * pi);

		r = rotation_d{.yaw = 1e9};
		r.clip();
		EXPECT_GE(r.yaw, 0.0);
		EXPECT_LT(r.yaw, 2 * pi);
	}

	TEST(math_rotation, clip_terminates_for_infinite_yaw) {
		auto r = rotation_d{.yaw = std::numeric_limits<double>::infinity()};
		r.clip();
		EXPECT_TRUE(std::isnan(r.yaw));
	}

	TEST(math_rotation, direction_clipped_does_not_modify) {
		rotation const r{.yaw = 0, .pitch = 3.f};
		EXPECT_TRUE(near(r.direction_clipped(), glm::vec3(0, 1, 0)));
		EXPECT_FLOAT_EQ(r.pitch, 3.f);
	}

	TEST(math_rotation, addition) {
		auto const sum = rotation{.yaw = 1, .pitch = 2} + rotation{.yaw = .5f, .pitch = -1};
		EXPECT_FLOAT_EQ(sum.yaw, 1.5f);
		EXPECT_FLOAT_EQ(sum.pitch, 1.f);
	}

	// ---- Newell normal ---------------------------------------------------

	TEST(math_newell_normal, counter_clockwise_triangle_faces_up) {
		std::array const vertices{glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};
		std::array<size_t, 3> const face{0, 1, 2};
		EXPECT_TRUE(near(*newell_normal(vertices, face), glm::dvec3(0, 0, 1)));
	}

	TEST(math_newell_normal, winding_flips_normal) {
		std::array const vertices{glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};
		std::array<size_t, 3> const face{0, 2, 1};
		EXPECT_TRUE(near(*newell_normal(vertices, face), glm::dvec3(0, 0, -1)));
	}

	TEST(math_newell_normal, uses_face_indices) {
		std::array const vertices{
		    glm::vec3(9, 9, 9),  // unused
		    glm::vec3(0, 0, 0), glm::vec3(0, 0, 2), glm::vec3(0, 2, 2), glm::vec3(0, 2, 0),
		};
		std::array<size_t, 4> const face{1, 2, 3, 4};
		EXPECT_TRUE(near(*newell_normal(vertices, face), glm::dvec3(-1, 0, 0)));
	}

	TEST(math_newell_normal, degenerate_face_has_no_normal) {
		std::array const vertices{glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), glm::vec3(2, 2, 2)};
		std::array<size_t, 3> const line{0, 1, 2};
		std::array<size_t, 3> const point{1, 1, 1};
		std::array<size_t, 2> const segment{0, 1};
		EXPECT_FALSE(newell_normal(vertices, line).has_value());
		EXPECT_FALSE(newell_normal(vertices, point).has_value());
		EXPECT_FALSE(newell_normal(vertices, segment).has_value());
		EXPECT_FALSE(newell_normal(vertices, std::span<size_t const>{}).has_value());
	}

	// triangle with its longest edge along X, `height` above it
	std::array<glm::vec3, 3> thin_triangle(float scale, float height, glm::vec3 offset = {}) {
		return {
		    offset + glm::vec3(0, 0, 0) * scale,
		    offset + glm::vec3(1, 0, 0) * scale,
		    offset + glm::vec3(.5f, height, 0) * scale,
		};
	}

	TEST(math_newell_normal, thin_face_is_degenerate) {
		std::array<size_t, 3> const face{0, 1, 2};
		auto const sliver = thin_triangle(1, 1e-7f);
		EXPECT_FALSE(newell_normal(sliver, face).has_value());
	}

	TEST(math_newell_normal, narrow_face_is_not_degenerate) {
		std::array<size_t, 3> const face{0, 1, 2};
		auto const narrow = thin_triangle(1, 1e-5f);
		auto const normal = newell_normal(narrow, face);
		ASSERT_TRUE(normal.has_value());
		EXPECT_TRUE(near(*normal, glm::dvec3(0, 0, 1)));
	}

	TEST(math_newell_normal, tolerance_does_not_depend_on_size) {
		std::array<size_t, 3> const face{0, 1, 2};
		for (auto const scale : {1e-3f, 1.f, 1e3f}) {
			EXPECT_FALSE(newell_normal(thin_triangle(scale, 1e-7f), face).has_value()) << "scale " << scale;
			EXPECT_TRUE(newell_normal(thin_triangle(scale, 1e-5f), face).has_value()) << "scale " << scale;
		}
	}

	TEST(math_newell_normal, tolerance_is_configurable) {
		std::array<size_t, 3> const face{0, 1, 2};
		auto const narrow = thin_triangle(1, 1e-3f);
		EXPECT_TRUE(newell_normal(narrow, face).has_value());
		EXPECT_FALSE(newell_normal(narrow, face, 1e-2).has_value());

		auto const sliver = thin_triangle(1, 1e-7f);
		EXPECT_TRUE(newell_normal(sliver, face, 0).has_value());
	}

	TEST(math_newell_normal, face_far_from_origin) {
		std::array<size_t, 3> const face{0, 1, 2};
		auto const small = thin_triangle(1e-2f, 1, glm::vec3(1000, -2000, 500));
		auto const normal = newell_normal(small, face);
		ASSERT_TRUE(normal.has_value());
		EXPECT_TRUE(near(*normal, glm::dvec3(0, 0, 1)));
	}

	TEST(math_newell_normal, tilted_quad) {
		std::array const vertices{
		    glm::vec3(0, 0, 0),
		    glm::vec3(1, 0, 0),
		    glm::vec3(1, 1, 1),
		    glm::vec3(0, 1, 1),
		};
		std::array<size_t, 4> const face{0, 1, 2, 3};
		auto const s = 1 / std::sqrt(2.0);
		EXPECT_TRUE(near(*newell_normal(vertices, face), glm::dvec3(0, -s, s)));
	}
}  // namespace pyxel::testing
