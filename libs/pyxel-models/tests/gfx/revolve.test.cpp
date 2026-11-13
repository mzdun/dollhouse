// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/gfx/solid.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pyxel::gfx::testing {
	namespace {
		using namespace math::literals;
		using profile = std::vector<solid::profile_point>;

		// divergence theorem; positive for closed solids with outward faces, in m^3
		double volume_of(solid const& obj) {
			auto const& vertices = obj.vertices();
			double result = 0.0;
			for (auto const& face : obj.faces()) {
				auto const origin = vertices[face[0]].to_glm();
				for (size_t index = 1; index + 1 < face.size(); ++index) {
					result += glm::dot(origin,
					                   glm::cross(vertices[face[index]].to_glm(), vertices[face[index + 1]].to_glm()));
				}
			}
			return result / 6.0;
		}

		// every edge is walked exactly once each way
		bool is_closed(solid const& obj) {
			std::map<std::pair<size_t, size_t>, int> edges{};
			for (auto const& face : obj.faces()) {
				for (size_t index = 0; index < face.size(); ++index)
					++edges[{face[index], face[(index + 1) % face.size()]}];
			}
			return std::ranges::all_of(edges, [&](auto const& entry) {
				auto const& [edge, count] = entry;
				auto const reverse = edges.find({edge.second, edge.first});
				return count == 1 && reverse != edges.end() && reverse->second == 1;
			});
		}

		// area of a regular polygon with the given circumradius, in m^2
		double polygon_area(size_t sides, double radius) {
			auto const n = static_cast<double>(sides);
			return n / 2 * radius * radius * std::sin(2 * std::numbers::pi / n);
		}

		constexpr double tolerance = 1e-12;

		profile const prism_profile{{0_mm, 0_mm}, {5_cm, 0_mm}, {5_cm, 30_cm}, {0_mm, 30_cm}};
		profile const pyramid_profile{{0_mm, 0_mm}, {5_cm, 0_mm}, {0_mm, 30_cm}};
		profile const tube_profile{{3_cm, 0_mm}, {5_cm, 0_mm}, {5_cm, 30_cm}, {3_cm, 30_cm}};
	}  // namespace

	TEST(gfx_revolve, prism) {
		auto const obj = solid::revolve(prism_profile, 6);
		ASSERT_TRUE(obj.is_valid());
		EXPECT_EQ(obj.vertices().size(), 12u);
		EXPECT_EQ(obj.faces().size(), 8u);  // 6 sides, 2 caps
		EXPECT_TRUE(is_closed(obj));
		EXPECT_NEAR(volume_of(obj), polygon_area(6, .05) * .3, tolerance);
	}

	TEST(gfx_revolve, caps_are_single_faces) {
		auto const obj = solid::revolve(prism_profile, 6);
		auto const hexagons = std::ranges::count_if(obj.faces(), [](auto const& face) { return face.size() == 6; });
		EXPECT_EQ(hexagons, 2);
	}

	TEST(gfx_revolve, pyramid) {
		auto const obj = solid::revolve(pyramid_profile, 4);
		ASSERT_TRUE(obj.is_valid());
		EXPECT_EQ(obj.vertices().size(), 5u);
		EXPECT_EQ(obj.faces().size(), 5u);  // 4 triangles, 1 base
		EXPECT_TRUE(is_closed(obj));
		EXPECT_NEAR(volume_of(obj), polygon_area(4, .05) * .3 / 3, tolerance);
		EXPECT_TRUE(std::ranges::any_of(obj.vertices(), [](offset const& v) { return near(v, offset::at_y(30_cm)); }));
	}

	TEST(gfx_revolve, origin_is_in_the_middle_of_the_base) {
		auto const obj = solid::revolve(prism_profile, 8);
		auto min = obj.vertices().front();
		auto max = min;
		for (auto const& vertex : obj.vertices()) {
			min = min_pt(min, vertex);
			max = max_pt(max, vertex);
		}
		EXPECT_TRUE(near(midpoint(min, max), offset::at_y(15_cm)));
		EXPECT_TRUE(near(min, offset{-5_cm, 0_mm, -5_cm}, 1_mm));
	}

	TEST(gfx_revolve, tube_has_a_hole) {
		auto const obj = solid::revolve(tube_profile, 12);
		ASSERT_TRUE(obj.is_valid());
		EXPECT_EQ(obj.vertices().size(), 48u);
		EXPECT_EQ(obj.faces().size(), 48u);
		EXPECT_TRUE(is_closed(obj));
		EXPECT_NEAR(volume_of(obj), (polygon_area(12, .05) - polygon_area(12, .03)) * .3, tolerance);
		EXPECT_FALSE(std::ranges::any_of(obj.vertices(), [](offset const& v) { return v.x == 0_m && v.z == 0_m; }));
	}

	TEST(gfx_revolve, direction_of_the_profile_does_not_matter) {
		for (auto const& source : {prism_profile, pyramid_profile, tube_profile}) {
			auto reversed = source;
			std::ranges::reverse(reversed);
			auto const forward = solid::revolve(source, 7);
			auto const backward = solid::revolve(reversed, 7);
			EXPECT_GT(volume_of(forward), 0);
			EXPECT_NEAR(volume_of(backward), volume_of(forward), tolerance);
			EXPECT_TRUE(is_closed(backward));
			EXPECT_EQ(backward.faces().size(), forward.faces().size());
		}
	}

	TEST(gfx_revolve, profile_need_not_start_on_the_axis) {
		// the prism profile, rotated so it starts at the top outer corner
		profile const rotated{{5_cm, 30_cm}, {0_mm, 30_cm}, {0_mm, 0_mm}, {5_cm, 0_mm}};
		auto const obj = solid::revolve(rotated, 6);
		EXPECT_EQ(obj.faces().size(), 8u);
		EXPECT_TRUE(is_closed(obj));
		EXPECT_NEAR(volume_of(obj), polygon_area(6, .05) * .3, tolerance);
	}

	TEST(gfx_revolve, double_cone) {
		// a spindle: both tips on the axis, no caps
		profile const spindle{{0_mm, 0_mm}, {5_cm, 10_cm}, {0_mm, 20_cm}};
		auto const obj = solid::revolve(spindle, 8);
		EXPECT_EQ(obj.vertices().size(), 10u);
		EXPECT_EQ(obj.faces().size(), 16u);
		EXPECT_TRUE(is_closed(obj));
		EXPECT_NEAR(volume_of(obj), 2 * polygon_area(8, .05) * .1 / 3, tolerance);
	}

	TEST(gfx_revolve, works_with_operations) {
		auto const outer = solid::revolve(prism_profile, 16);
		auto const inner = solid::revolve(profile{{0_mm, 1_cm}, {3_cm, 1_cm}, {3_cm, 31_cm}, {0_mm, 31_cm}}, 16);
		auto const cup = cleanup(subtract(outer, inner));
		EXPECT_NEAR(volume_of(cup), volume_of(outer) - polygon_area(16, .03) * .29, tolerance);
	}

	TEST(gfx_revolve, rejects_bad_input) {
		EXPECT_THROW(solid::revolve(prism_profile, 2), std::invalid_argument);
		EXPECT_THROW(solid::revolve(profile{{0_mm, 0_mm}, {-1_cm, 0_mm}, {0_mm, 1_cm}}, 6), std::invalid_argument);
		// flat: all on the axis, or only two points
		EXPECT_THROW(solid::revolve(profile{{0_mm, 0_mm}, {0_mm, 1_cm}}, 6), std::invalid_argument);
		EXPECT_THROW(solid::revolve(profile{{1_cm, 0_mm}, {2_cm, 1_cm}}, 6), std::invalid_argument);
		EXPECT_THROW(solid::revolve(profile{}, 6), std::invalid_argument);
	}
}  // namespace pyxel::gfx::testing
