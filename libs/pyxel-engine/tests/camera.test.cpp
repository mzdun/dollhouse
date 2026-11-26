// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <pyxel/camera.hpp>
#include <pyxel/math/glm.hpp>

namespace pyxel::testing {
	namespace {
		using namespace math::literals;

		glm::vec3 project(camera const& cam, float aspect, glm::vec3 point) {
			auto const clip = cam.projection(aspect) * cam.view() * glm::vec4{point, 1.0f};
			return glm::vec3{clip} / clip.w;
		}

		std::array<glm::vec3, 8> corners(AABB const& box) {
			std::array<glm::vec3, 8> result{};
			for (size_t index = 0; index < result.size(); ++index) {
				result[index] = {(index & 1) ? box.max.x : box.min.x, (index & 2) ? box.max.y : box.min.y,
				                 (index & 4) ? box.max.z : box.min.z};
			}
			return result;
		}

		struct framing_case {
			AABB box{};
			float aspect{};
		};

		class camera_framing : public ::testing::TestWithParam<framing_case> {};

		TEST_P(camera_framing, middle_in_the_middle) {
			auto const& [box, aspect] = GetParam();
			auto const cam = camera::framing(box, aspect);
			auto const ndc = project(cam, aspect, box.mid());
			EXPECT_NEAR(ndc.x, 0.0f, 1e-5f);
			EXPECT_NEAR(ndc.y, 0.0f, 1e-5f);
			EXPECT_GT(ndc.z, 0.0f);
			EXPECT_LT(ndc.z, 1.0f);
		}

		TEST_P(camera_framing, box_inside_the_frustum) {
			auto const& [box, aspect] = GetParam();
			if (box.empty()) return;  // its corners are at infinity
			auto const cam = camera::framing(box, aspect);
			for (auto const& corner : corners(box)) {
				auto const ndc = project(cam, aspect, corner);
				EXPECT_GE(ndc.x, -1.0f) << corner.x << ", " << corner.y << ", " << corner.z;
				EXPECT_LE(ndc.x, 1.0f) << corner.x << ", " << corner.y << ", " << corner.z;
				EXPECT_GE(ndc.y, -1.0f) << corner.x << ", " << corner.y << ", " << corner.z;
				EXPECT_LE(ndc.y, 1.0f) << corner.x << ", " << corner.y << ", " << corner.z;
				EXPECT_GE(ndc.z, 0.0f) << corner.x << ", " << corner.y << ", " << corner.z;
				EXPECT_LE(ndc.z, 1.0f) << corner.x << ", " << corner.y << ", " << corner.z;
			}
		}

		TEST_P(camera_framing, eye_on_the_diagonal) {
			auto const& [box, aspect] = GetParam();
			auto const cam = camera::framing(box, aspect);
			auto const offset = cam.eye - box.mid();
			EXPECT_GT(offset.x, 0.0f);
			EXPECT_NEAR(offset.x, offset.y, 1e-4f * offset.x);
			EXPECT_NEAR(offset.x, offset.z, 1e-4f * offset.x);
			EXPECT_EQ(cam.target, box.mid());
		}

		INSTANTIATE_TEST_SUITE_P(boxes,
		                         camera_framing,
		                         ::testing::Values(framing_case{{.min = {-1, -1, -1}, .max = {1, 1, 1}}, 16.0f / 10.0f},
		                                           framing_case{{.min = {-1, 0, 2}, .max = {3, 4, 4}}, 1.0f},
		                                           framing_case{{.min = {0, 0, 0}, .max = {0.8f, 2.0f, 0.4f}},
		                                                        9.0f / 16.0f},
		                                           framing_case{{.min = {-5, 0, -5}, .max = {5, 0, 5}}, 4.0f / 3.0f},
		                                           framing_case{{.min = {2, 2, 2}, .max = {2, 2, 2}}, 16.0f / 9.0f},
		                                           framing_case{{}, 16.0f / 9.0f}));

		TEST(camera, doom_fov) {
			// 90° across 4:3
			auto const half_x = std::atan(std::tan(doom_fov_y().count() / 2) * 4.0 / 3.0);
			EXPECT_NEAR(half_x * 2, math::radians{90_deg}.count(), 1e-12);
			EXPECT_NEAR(math::degrees{doom_fov_y()}.count(), 73.74, 0.01);
		}

		TEST(camera, wider_box_needs_more_distance_in_portrait) {
			AABB const box{.min = {-1, -1, -1}, .max = {1, 1, 1}};
			auto const landscape = camera::framing(box, 16.0f / 9.0f);
			auto const portrait = camera::framing(box, 9.0f / 16.0f);
			EXPECT_GT(glm::length(portrait.eye - box.mid()), glm::length(landscape.eye - box.mid()));
		}
	}  // namespace
}  // namespace pyxel::testing
