// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/materials.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/transform.hpp>
#include <stdexcept>
#include <vector>

// NaN breaks the ordering of the vertex maps (std::map with lexicographic
// less), and NaN or infinity cannot be exported; both are rejected, where
// the values become geometry.
namespace pyxel::gfx::testing {
	using namespace math::literals;

	namespace {
		constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
		constexpr auto inf = std::numeric_limits<double>::infinity();

		length meters(double value) { return length{value}; }

		glm::dmat4 with(double value) {
			glm::dmat4 matrix{1.0};
			matrix[3][0] = value;
			return matrix;
		}
	}  // namespace

	// ---- solid -------------------------------------------------------------

	TEST(gfx_finite, add_vert_rejects_nan_and_inf) {
		for (auto const bad : {nan, inf, -inf}) {
			solid obj{};
			obj.add_vert(1_cm, 0_cm, 0_cm);
			EXPECT_THROW(obj.add_vert(meters(bad), 0_cm, 0_cm), std::invalid_argument) << bad;
			EXPECT_THROW(obj.add_vert(0_cm, meters(bad), 0_cm), std::invalid_argument) << bad;
			EXPECT_THROW(obj.add_vert(0_cm, 0_cm, meters(bad)), std::invalid_argument) << bad;
			EXPECT_EQ(obj.vertices().size(), 1u) << bad;
		}
	}

	TEST(gfx_finite, nan_does_not_alias_another_vertex) {
		// the original bug: NaN was "equivalent" to any key of the map, and
		// got the index of an existing vertex
		solid obj{};
		auto const first = obj.add_vert(1_cm, 0_cm, 0_cm);
		EXPECT_THROW(obj.add_vert(meters(nan), 0_cm, 0_cm), std::invalid_argument);
		auto const second = obj.add_vert(2_cm, 0_cm, 0_cm);
		EXPECT_EQ(first, 0u);
		EXPECT_EQ(second, 1u);
	}

	TEST(gfx_finite, constructor_rejects_nan) {
		std::vector<offset> vertices{{}, {.x = meters(nan)}, {.y = 1_cm}};
		EXPECT_THROW((solid{vertices, {{0, 1, 2}}}), std::invalid_argument);
	}

	TEST(gfx_finite, transform_rejects_inf) {
		auto const cube = solid::cube(1_cm, 1_cm, 1_cm);
		EXPECT_THROW(cube.transform(with(inf)), std::invalid_argument);
		EXPECT_THROW(cube.transform(with(nan)), std::invalid_argument);
	}

	// ---- basic solids --------------------------------------------------------

	TEST(gfx_finite, cube_rejects_nan_and_inf) {
		EXPECT_THROW(solid::cube(meters(inf), 1_cm, 1_cm), std::invalid_argument);
		EXPECT_THROW(solid::cube(1_cm, meters(nan), 1_cm), std::invalid_argument);
		EXPECT_THROW(solid::cube(1_cm, 1_cm, meters(-inf)), std::invalid_argument);
	}

	TEST(gfx_finite, messages_name_the_problem) {
		try {
			solid::cube(meters(inf), 1_cm, 1_cm);
			FAIL() << "no exception";
		} catch (std::invalid_argument const& err) {
			EXPECT_STREQ(err.what(), "cube: the width must be finite");
		}
		try {
			solid::cube(meters(nan), 1_cm, 1_cm);
			FAIL() << "no exception";
		} catch (std::invalid_argument const& err) {
			EXPECT_STREQ(err.what(), "cube: the width must be larger than zero");
		}
	}

	TEST(gfx_finite, round_shapes_reject_nan_and_inf) {
		EXPECT_THROW(solid::prism(1_cm, meters(inf), 8), std::invalid_argument);
		EXPECT_THROW(solid::pyramid(meters(nan), 1_cm, 8), std::invalid_argument);
		EXPECT_THROW(solid::sphere(meters(inf), 8), std::invalid_argument);
		EXPECT_THROW(solid::oval_sphere(1_cm, meters(nan), std::nullopt, 8), std::invalid_argument);
	}

	TEST(gfx_finite, rounding_radius_rejects_nan_and_inf) {
		// `radius < 0` let NaN through
		EXPECT_THROW(solid::rounded_cube(1_cm, 1_cm, 1_cm, meters(nan), 2), std::invalid_argument);
		EXPECT_THROW(solid::rounded_cube(1_cm, 1_cm, 1_cm, meters(inf), 2), std::invalid_argument);
		EXPECT_THROW(solid::oval_rounded_cube(1_cm, 1_cm, 1_cm, 1_mm, meters(nan), std::nullopt, 2),
		             std::invalid_argument);
		EXPECT_THROW(solid::oval_rounded_cube(1_cm, 1_cm, 1_cm, 1_mm, std::nullopt, meters(inf), 2),
		             std::invalid_argument);
	}

	TEST(gfx_finite, revolve_rejects_nan_and_inf) {
		for (auto const bad : {nan, inf}) {
			std::array<solid::profile_point, 4> const across{
			    {{0_cm, 0_cm}, {meters(bad), 0_cm}, {1_cm, 1_cm}, {0_cm, 1_cm}}};
			EXPECT_THROW(solid::revolve(across, 8), std::invalid_argument) << bad;
			std::array<solid::profile_point, 4> const along{
			    {{0_cm, 0_cm}, {1_cm, meters(bad)}, {1_cm, 1_cm}, {0_cm, 1_cm}}};
			EXPECT_THROW(solid::revolve(along, 8), std::invalid_argument) << bad;
		}
	}

	// ---- transforms -------------------------------------------------------------

	TEST(gfx_finite, scale_rejects_nan_and_inf) {
		EXPECT_THROW(scale(identity_matrix, nan), std::invalid_argument);
		EXPECT_THROW(scale(identity_matrix, 1.0, inf), std::invalid_argument);
		EXPECT_THROW(scale(identity_matrix, 1.0, 1.0, -inf), std::invalid_argument);
	}

	TEST(gfx_finite, rotate_rejects_nan_and_inf) {
		EXPECT_THROW(rotate(identity_matrix, deg(90), {inf, 0, 0}), std::invalid_argument);
		EXPECT_THROW(rotate(identity_matrix, deg(90), {0, nan, 0}), std::invalid_argument);
		EXPECT_THROW(rotate(identity_matrix, rad(inf), {0, 0, 1}), std::invalid_argument);
		EXPECT_THROW(rotate(identity_matrix, rad(nan), {0, 0, 1}), std::invalid_argument);
	}

	// ---- sink ---------------------------------------------------------------------

	TEST(gfx_finite, sink_rejects_non_finite_matrix_and_stays_unchanged) {
		sink out{};
		auto const cube = solid::cube(1_cm, 1_cm, 1_cm);
		EXPECT_THROW(out.add_solid(cube, with(nan)), std::invalid_argument);
		EXPECT_THROW(out.add_solid(cube, with(inf)), std::invalid_argument);
		// no empty mesh left behind by the failed bakes
		EXPECT_TRUE(out.at(sink::root).meshes.empty());

		out.add_solid(cube, identity_matrix);
		EXPECT_THROW(out.add_solid(cube, with(nan)), std::invalid_argument);
		auto const& meshes = out.at(sink::root).meshes;
		ASSERT_EQ(meshes.size(), 1u);
		EXPECT_EQ(meshes.begin()->second.faces().size(), 6u);
	}

	TEST(gfx_finite, sink_rejects_overflow) {
		// finite matrix, finite vertices, infinite product: 10 m * 1e308
		sink out{};
		auto const cube = solid::cube(meters(10), meters(10), meters(10));
		EXPECT_THROW(out.add_solid(cube, scale(identity_matrix, 1e308)), std::invalid_argument);
		EXPECT_TRUE(out.at(sink::root).meshes.empty());
	}

	// ---- materials ----------------------------------------------------------------

	TEST(gfx_finite, paint_rejects_nan_and_out_of_range) {
		materials repo{};
		auto const paint = [](float roughness, float metallic) {
			return materials::paint_opts{.color = rgb(255, 0, 0), .roughness = roughness, .metallic = metallic};
		};
		auto const fnan = std::numeric_limits<float>::quiet_NaN();
		EXPECT_THROW(repo.register_paint(paint(fnan, 0.0f)), std::invalid_argument);
		EXPECT_THROW(repo.register_paint(paint(1.0f, fnan)), std::invalid_argument);
		EXPECT_THROW(repo.register_paint(paint(-0.1f, 0.0f)), std::invalid_argument);
		EXPECT_THROW(repo.register_paint(paint(1.0f, 1.5f)), std::invalid_argument);
		EXPECT_NO_THROW(repo.register_paint(paint(0.0f, 1.0f)));
	}
}  // namespace pyxel::gfx::testing
