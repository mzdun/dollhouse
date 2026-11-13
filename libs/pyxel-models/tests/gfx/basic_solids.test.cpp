// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/gfx/solid.hpp>
#include <stdexcept>
#include <utility>

namespace pyxel::gfx::testing {
	namespace {
		using namespace math::literals;
		using std::numbers::pi;

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

		// every face lies in one plane
		bool is_flat(solid const& obj) {
			auto const& vertices = obj.vertices();
			return std::ranges::all_of(obj.faces(), [&](auto const& face) {
				auto const a = vertices[face[0]].to_glm();
				auto const normal =
				    glm::normalize(glm::cross(vertices[face[1]].to_glm() - a, vertices[face[2]].to_glm() - a));
				return std::ranges::all_of(face, [&](size_t index) {
					return std::abs(glm::dot(vertices[index].to_glm() - a, normal)) < 1e-9;
				});
			});
		}

		std::pair<offset, offset> bounds_of(solid const& obj) {
			auto min = obj.vertices().front();
			auto max = min;
			for (auto const& vertex : obj.vertices()) {
				min = min_pt(min, vertex);
				max = max_pt(max, vertex);
			}
			return {min, max};
		}

		double polygon_area(size_t sides, double radius) {
			auto const n = static_cast<double>(sides);
			return n / 2 * radius * radius * std::sin(2 * pi / n);
		}

		// exact volume of a box with rounded edges and corners
		double rounded_volume(double w, double h, double d, double r) {
			auto const a = w - 2 * r;
			auto const b = h - 2 * r;
			auto const c = d - 2 * r;
			return a * b * c + 2 * r * (a * b + a * c + b * c) + pi * r * r * (a + b + c) + 4. / 3 * pi * r * r * r;
		}

		// the same, with elliptic edges and corners
		double oval_volume(double w, double h, double d, double rx, double ry, double rz) {
			auto const a = w - 2 * rx;
			auto const b = h - 2 * ry;
			auto const c = d - 2 * rz;
			return a * b * c + 2 * (rx * b * c + ry * a * c + rz * a * b) +
			       pi * (rx * ry * c + rx * rz * b + ry * rz * a) + 4. / 3 * pi * rx * ry * rz;
		}

		void expect_solid(solid const& obj) {
			EXPECT_TRUE(obj.is_valid());
			EXPECT_TRUE(is_closed(obj));
			EXPECT_TRUE(is_flat(obj));
			EXPECT_GT(volume_of(obj), 0);
		}

		constexpr double tolerance = 1e-12;
	}  // namespace

	TEST(gfx_basic_solids, cube) {
		auto const obj = solid::cube(20_cm, 30_cm, 40_cm);
		expect_solid(obj);
		EXPECT_EQ(obj.vertices().size(), 8u);
		EXPECT_EQ(obj.faces().size(), 6u);
		EXPECT_NEAR(volume_of(obj), .2 * .3 * .4, tolerance);
		auto const [min, max] = bounds_of(obj);
		EXPECT_TRUE(near(min, offset{}));
		EXPECT_TRUE(near(max, offset{20_cm, 30_cm, 40_cm}));
	}

	TEST(gfx_basic_solids, prism) {
		auto const obj = solid::prism(30_cm, 5_cm, 8);
		expect_solid(obj);
		EXPECT_EQ(obj.vertices().size(), 16u);
		EXPECT_EQ(obj.faces().size(), 10u);
		EXPECT_NEAR(volume_of(obj), polygon_area(8, .05) * .3, tolerance);
		EXPECT_TRUE(near(bounds_of(obj).first, offset{-5_cm, 0_cm, -5_cm}, 1_mm));
	}

	TEST(gfx_basic_solids, pyramid) {
		auto const obj = solid::pyramid(30_cm, 5_cm, 4);
		expect_solid(obj);
		EXPECT_EQ(obj.vertices().size(), 5u);
		EXPECT_EQ(obj.faces().size(), 5u);
		EXPECT_NEAR(volume_of(obj), polygon_area(4, .05) * .3 / 3, tolerance);
		EXPECT_TRUE(near(bounds_of(obj).second.replace({.x = 0_m, .z = 0_m}), offset::at_y(30_cm)));
	}

	TEST(gfx_basic_solids, sphere) {
		auto const obj = solid::sphere(10_cm, 8);
		expect_solid(obj);
		// 8 longitudes, 4 bands of latitude: 3 rings of 8, and 2 poles
		EXPECT_EQ(obj.vertices().size(), 26u);
		EXPECT_EQ(obj.faces().size(), 32u);
		for (auto const& vertex : obj.vertices())
			EXPECT_NEAR(glm::length(vertex.to_glm()), .1, tolerance);
		auto const [min, max] = bounds_of(obj);
		EXPECT_TRUE(near(min.replace({.x = 0_m, .z = 0_m}), offset::at_y(-10_cm)));
		EXPECT_TRUE(near(max.replace({.x = 0_m, .z = 0_m}), offset::at_y(10_cm)));
	}

	TEST(gfx_basic_solids, sphere_gets_closer_with_more_longitudes) {
		auto const exact = 4. / 3 * pi * .001;
		auto const coarse = volume_of(solid::sphere(10_cm, 8));
		auto const fine = volume_of(solid::sphere(10_cm, 64));
		EXPECT_LT(coarse, fine);
		EXPECT_LT(fine, exact);
		EXPECT_GT(fine, .99 * exact);
	}

	TEST(gfx_basic_solids, odd_longitudes_round_the_latitudes_down) {
		auto const obj = solid::sphere(10_cm, 7);
		expect_solid(obj);
		EXPECT_EQ(obj.faces().size(), 7u * 3u);
	}

	TEST(gfx_basic_solids, rounded_cube) {
		auto const exact = rounded_volume(.2, .3, .4, .05);
		double previous = 0.0;
		for (size_t segments : {1u, 3u, 8u}) {
			auto const obj = solid::rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, segments);
			expect_solid(obj);
			auto const k = segments;
			// 8 corners of (k + 1)^2 - k vertices; 6 flat faces, 12 edges of
			// k strips and 8 corners of k^2 patches
			EXPECT_EQ(obj.vertices().size(), 8 * ((k + 1) * (k + 1) - k));
			EXPECT_EQ(obj.faces().size(), 6 + 12 * k + 8 * k * k);

			auto const [min, max] = bounds_of(obj);
			EXPECT_TRUE(near(min, offset{}));
			EXPECT_TRUE(near(max, offset{20_cm, 30_cm, 40_cm}));

			// the corners and edges are inscribed, so the volume grows
			// towards the exact one with more segments
			auto const volume = volume_of(obj);
			EXPECT_LT(volume, exact);
			EXPECT_GT(volume, previous);
			previous = volume;
		}
		EXPECT_GT(previous, .995 * exact);
	}

	TEST(gfx_basic_solids, rounded_cube_clamps_the_radius) {
		// the radius stops at half of the width: the ends along x are half
		// cylinders, and there are no flat faces facing x
		auto const obj = solid::rounded_cube(20_cm, 30_cm, 40_cm, 1_m, 4);
		expect_solid(obj);
		auto const same = solid::rounded_cube(20_cm, 30_cm, 40_cm, 10_cm, 4);
		EXPECT_EQ(obj.vertices(), same.vertices());
		EXPECT_EQ(obj.faces(), same.faces());
		EXPECT_LT(obj.faces().size(), 6 + 12 * 4 + 8 * 16u);
		EXPECT_NEAR(volume_of(obj), volume_of(same), tolerance);
	}

	TEST(gfx_basic_solids, rounded_cube_of_equal_sides_and_radius_is_a_sphere) {
		auto const obj = solid::rounded_cube(20_cm, 20_cm, 20_cm, 10_cm, 4);
		expect_solid(obj);
		for (auto const& vertex : obj.vertices())
			EXPECT_NEAR(glm::length(vertex.to_glm() - glm::dvec3{.1, .1, .1}), .1, tolerance);
	}

	TEST(gfx_basic_solids, rounded_cube_with_no_radius_is_a_cube) {
		auto const obj = solid::rounded_cube(20_cm, 30_cm, 40_cm, 0_cm, 4);
		auto const box = solid::cube(20_cm, 30_cm, 40_cm);
		EXPECT_EQ(obj.vertices(), box.vertices());
		EXPECT_EQ(obj.faces(), box.faces());
	}

	TEST(gfx_basic_solids, oval_sphere) {
		auto const obj = solid::oval_sphere(10_cm, 20_cm, 5_cm, 16);
		expect_solid(obj);
		auto const round = solid::sphere(10_cm, 16);
		EXPECT_EQ(obj.vertices().size(), round.vertices().size());
		EXPECT_EQ(obj.faces(), round.faces());
		for (auto const& vertex : obj.vertices()) {
			auto const p = vertex.to_glm() / glm::dvec3{.1, .2, .05};
			EXPECT_NEAR(glm::dot(p, p), 1.0, 1e-9);
		}
		auto const [min, max] = bounds_of(obj);
		EXPECT_TRUE(near(min, offset{-10_cm, -20_cm, -5_cm}, 1_cm));
		EXPECT_NEAR(min.y.count(), -.2, tolerance);
		EXPECT_NEAR(max.y.count(), .2, tolerance);
		// stretching scales the volume by the ratios of the radii
		EXPECT_NEAR(volume_of(obj), volume_of(round) * 2 * .5, 1e-9);
	}

	TEST(gfx_basic_solids, oval_sphere_missing_radii_go_back_to_rx) {
		auto const round = solid::sphere(10_cm, 8);
		auto const same = solid::oval_sphere(10_cm, std::nullopt, std::nullopt, 8);
		EXPECT_EQ(same.vertices(), round.vertices());
		EXPECT_EQ(same.faces(), round.faces());

		auto const only_y = solid::oval_sphere(10_cm, 20_cm, std::nullopt, 8);
		auto const both = solid::oval_sphere(10_cm, 20_cm, 10_cm, 8);
		EXPECT_EQ(only_y.vertices(), both.vertices());
	}

	TEST(gfx_basic_solids, oval_rounded_cube) {
		auto const exact = oval_volume(.2, .3, .4, .08, .05, .12);
		double previous = 0.0;
		for (size_t segments : {1u, 3u, 8u}) {
			auto const obj = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 8_cm, 5_cm, 12_cm, segments);
			expect_solid(obj);
			auto const k = segments;
			EXPECT_EQ(obj.vertices().size(), 8 * ((k + 1) * (k + 1) - k));
			EXPECT_EQ(obj.faces().size(), 6 + 12 * k + 8 * k * k);

			auto const [min, max] = bounds_of(obj);
			EXPECT_TRUE(near(min, offset{}));
			EXPECT_TRUE(near(max, offset{20_cm, 30_cm, 40_cm}));

			auto const volume = volume_of(obj);
			EXPECT_LT(volume, exact);
			EXPECT_GT(volume, previous);
			previous = volume;
		}
		EXPECT_GT(previous, .995 * exact);
	}

	TEST(gfx_basic_solids, oval_rounded_cube_missing_radii_go_back_to_rx) {
		auto const round = solid::rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, 3);
		auto const same = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, std::nullopt, std::nullopt, 3);
		EXPECT_EQ(same.vertices(), round.vertices());
		EXPECT_EQ(same.faces(), round.faces());

		auto const only_y = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, 8_cm, std::nullopt, 3);
		auto const both = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, 8_cm, 5_cm, 3);
		EXPECT_EQ(only_y.vertices(), both.vertices());
	}

	TEST(gfx_basic_solids, oval_rounded_cube_clamps_each_radius_separately) {
		// rx stops at 10 cm, rz (missing, so rx) at 20 cm, ry stays 5 cm
		auto const obj = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 1_m, 5_cm, std::nullopt, 4);
		auto const clamped = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 10_cm, 5_cm, 20_cm, 4);
		expect_solid(obj);
		EXPECT_EQ(obj.vertices(), clamped.vertices());
		EXPECT_EQ(obj.faces(), clamped.faces());
		// the circular version would clamp all of them to 10 cm
		EXPECT_NE(obj.vertices(), solid::rounded_cube(20_cm, 30_cm, 40_cm, 1_m, 4).vertices());
	}

	TEST(gfx_basic_solids, oval_rounded_cube_with_some_radii_zero) {
		// rounded only around the y axis: a box with rounded vertical edges
		auto const obj = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 5_cm, 0_cm, 5_cm, 8);
		expect_solid(obj);
		auto const exact = oval_volume(.2, .3, .4, .05, 0, .05);
		EXPECT_LT(volume_of(obj), exact);
		EXPECT_GT(volume_of(obj), .995 * exact);

		auto const box = solid::oval_rounded_cube(20_cm, 30_cm, 40_cm, 0_cm, 0_cm, 0_cm, 8);
		EXPECT_EQ(box.vertices(), solid::cube(20_cm, 30_cm, 40_cm).vertices());
	}

	TEST(gfx_basic_solids, work_with_operations) {
		auto const result =
		    cleanup(subtract(solid::rounded_cube(20_cm, 20_cm, 20_cm, 2_cm, 3),
		                     solid::sphere(8_cm, 16).transform(translate(identity_matrix, 10_cm, 10_cm, 10_cm))));
		EXPECT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result),
		            volume_of(solid::rounded_cube(20_cm, 20_cm, 20_cm, 2_cm, 3)) - volume_of(solid::sphere(8_cm, 16)),
		            1e-9);
	}

	TEST(gfx_basic_solids, reject_bad_input) {
		EXPECT_THROW(solid::cube(0_cm, 1_cm, 1_cm), std::invalid_argument);
		EXPECT_THROW(solid::prism(1_cm, -1_cm, 8), std::invalid_argument);
		EXPECT_THROW(solid::prism(1_cm, 1_cm, 2), std::invalid_argument);
		EXPECT_THROW(solid::pyramid(0_cm, 1_cm, 8), std::invalid_argument);
		EXPECT_THROW(solid::sphere(1_cm, 3), std::invalid_argument);
		EXPECT_THROW(solid::sphere(0_cm, 8), std::invalid_argument);
		EXPECT_THROW(solid::rounded_cube(1_cm, 1_cm, 1_cm, -1_cm, 4), std::invalid_argument);
		EXPECT_THROW(solid::rounded_cube(1_cm, 1_cm, 1_cm, 1_mm, 0), std::invalid_argument);
		EXPECT_THROW(solid::oval_sphere(1_cm, 0_cm, std::nullopt, 8), std::invalid_argument);
		EXPECT_THROW(solid::oval_sphere(1_cm, std::nullopt, std::nullopt, 3), std::invalid_argument);
		EXPECT_THROW(solid::oval_rounded_cube(1_cm, 1_cm, 1_cm, 1_mm, -1_mm, std::nullopt, 4), std::invalid_argument);
	}
}  // namespace pyxel::gfx::testing
