// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/solid.hpp>
#include <pyxel/gfx/transform.hpp>
#include <stdexcept>
#include <vector>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	auto const I = identity_matrix;
	offset const P{1_m, 2_m, 3_m};

	::testing::AssertionResult maps(glm::dmat4 const& matrix, offset const& from, offset const& to) {
		auto const actual = from.apply(matrix);
		if (near(actual, to)) return ::testing::AssertionSuccess();
		return ::testing::AssertionFailure() << from << " went to " << actual << ", expected " << to;
	}

	TEST(gfx_transform, identity_keeps_points) { EXPECT_TRUE(maps(I, P, P)); }

	TEST(gfx_transform, translate) {
		EXPECT_TRUE(maps(translate(I, offset{1_m, -1_m, 5_m}), P, offset{2_m, 1_m, 8_m}));
		EXPECT_TRUE(maps(translate(I, 1_m), P, offset{2_m, 2_m, 3_m}));
		EXPECT_TRUE(maps(translate(I, 1_m, 1_m), P, offset{2_m, 3_m, 3_m}));
		EXPECT_TRUE(maps(translate(I, 1_m, 1_m, 1_m), P, offset{2_m, 3_m, 4_m}));
	}

	TEST(gfx_transform, scale_defaults_to_x) {
		EXPECT_TRUE(maps(scale(I, 2), P, offset{2_m, 4_m, 6_m}));
		EXPECT_TRUE(maps(scale(I, 2, 3), P, offset{2_m, 6_m, 6_m}));
		EXPECT_TRUE(maps(scale(I, 2, 3, 4), P, offset{2_m, 6_m, 12_m}));
		EXPECT_TRUE(maps(scale(I, 2, std::nullopt, 4), P, offset{2_m, 4_m, 12_m}));
	}

	TEST(gfx_transform, rotations_are_right_handed) {
		EXPECT_TRUE(maps(rotate_x(I, deg(90)), offset::at_y(1_m), offset::at_z(1_m)));
		EXPECT_TRUE(maps(rotate_y(I, deg(90)), offset::at_z(1_m), offset::at_x(1_m)));
		EXPECT_TRUE(maps(rotate_z(I, deg(90)), offset::at_x(1_m), offset::at_y(1_m)));
	}

	TEST(gfx_transform, rotate_around_any_axis) {
		// the axis is normalized
		EXPECT_TRUE(maps(rotate(I, deg(90), {0, 0, 5}), offset::at_x(1_m), offset::at_y(1_m)));
		// a third of a turn around the main diagonal cycles the axes
		EXPECT_TRUE(maps(rotate(I, deg(120), {1, 1, 1}), offset::at_x(1_m), offset::at_y(1_m)));
		EXPECT_THROW(rotate(I, deg(90), {0, 0, 0}), std::invalid_argument);
	}

	TEST(gfx_transform, rotate_around_pivot) {
		auto const pivot = offset::at_x(1_m);
		EXPECT_TRUE(maps(rotate_z(I, deg(90), pivot), offset::at_x(2_m), offset{1_m, 1_m, 0_m}));
		EXPECT_TRUE(maps(rotate_z(I, deg(90), pivot), pivot, pivot));
		EXPECT_TRUE(maps(rotate(I, deg(180), {0, 1, 0}, offset{1_m, 0_m, 1_m}), offset{}, offset{2_m, 0_m, 2_m}));
	}

	TEST(gfx_transform, later_calls_are_applied_later) {
		// moved to x = 1 m first, then rotated onto the y axis
		EXPECT_TRUE(maps(rotate_z(translate(I, 1_m), deg(90)), offset{}, offset::at_y(1_m)));
		// rotated first (the origin stays), then moved
		EXPECT_TRUE(maps(translate(rotate_z(I, deg(90)), 1_m), offset{}, offset::at_x(1_m)));
		// scaling after the move scales the move as well
		EXPECT_TRUE(maps(scale(translate(I, 1_m), 2), offset{}, offset::at_x(2_m)));
	}

	TEST(gfx_transform, operands_are_not_changed) {
		auto const matrix = translate(I, 1_m);
		auto const copy = matrix;
		[[maybe_unused]] auto const result = rotate_z(matrix, deg(90));
		EXPECT_EQ(matrix, copy);
	}

	// ---- solid::transform ------------------------------------------------

	solid triangle() {
		// separate statements: the order of evaluating arguments is unspecified
		solid obj{};
		auto const a = obj.add_vert(offset{});
		auto const b = obj.add_vert(offset::at_x(1_m));
		auto const c = obj.add_vert(offset::at_y(1_m));
		obj.add_face(a, b, c);
		return obj;
	}

	TEST(gfx_solid_transform, moves_every_vertex) {
		auto const moved = triangle().transform(translate(I, 0_m, 0_m, 1_m));
		ASSERT_EQ(moved.vertices().size(), 3u);
		EXPECT_TRUE(near(moved.vertices()[0], offset::at_z(1_m)));
		EXPECT_TRUE(near(moved.vertices()[1], offset{1_m, 0_m, 1_m}));
		EXPECT_TRUE(near(moved.vertices()[2], offset{0_m, 1_m, 1_m}));
		EXPECT_EQ(moved.faces(), triangle().faces());
	}

	TEST(gfx_solid_transform, keeps_the_original) {
		auto const original = triangle();
		[[maybe_unused]] auto const moved = original.transform(translate(I, 1_m));
		EXPECT_EQ(original.vertices(), triangle().vertices());
	}

	TEST(gfx_solid_transform, mirroring_reverses_faces) {
		auto const mirrored = triangle().transform(scale(I, -1, 1, 1));
		EXPECT_EQ(mirrored.faces(), (std::vector<std::vector<size_t>>{{2, 1, 0}}));

		// two mirrors make a rotation, which keeps the order
		auto const rotated = triangle().transform(scale(I, -1, -1, 1));
		EXPECT_EQ(rotated.faces(), triangle().faces());
	}

	TEST(gfx_solid_transform, finds_moved_vertices) {
		auto moved = triangle().transform(translate(I, 1_m));
		EXPECT_EQ(moved.add_vert(offset::at_x(2_m)), 1u);
	}
}  // namespace pyxel::gfx::testing
