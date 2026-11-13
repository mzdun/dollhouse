// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <type_traits>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	// ---- offset ----------------------------------------------------------

	TEST(gfx_offset, default_is_origin) { EXPECT_EQ(offset{}, (offset{0.0_m, 0.0_m, 0.0_m})); }

	TEST(gfx_offset, at_helpers_set_one_axis) {
		static_assert(offset::at_x(1.0_m) == offset{1.0_m, 0.0_m, 0.0_m});
		static_assert(offset::at_y(2.0_m) == offset{0.0_m, 2.0_m, 0.0_m});
		static_assert(offset::at_z(3.0_m) == offset{0.0_m, 0.0_m, 3.0_m});
	}

	TEST(gfx_offset, at_helpers_accept_other_units) {
		EXPECT_EQ(offset::at_x(25_cm), (offset{0.25_m, 0.0_m, 0.0_m}));
		EXPECT_EQ(offset::at_y(5_mm), (offset{0.0_m, 0.005_m, 0.0_m}));
	}

	TEST(gfx_offset, addition_and_subtraction) {
		static_assert(offset::at_x(1.0_m) + offset::at_y(2.0_m) + offset::at_z(3.0_m) == offset{1.0_m, 2.0_m, 3.0_m});
		static_assert(offset{1.0_m, 2.0_m, 3.0_m} - offset{0.5_m, 2.0_m, 4.0_m} == offset{0.5_m, 0.0_m, -1.0_m});
		static_assert(-offset{1.0_m, -2.0_m, 0.0_m} == offset{-1.0_m, 2.0_m, 0.0_m});
	}

	TEST(gfx_offset, compound_addition_and_subtraction) {
		auto off = offset::at_x(1.0_m);
		off += offset::at_z(2.0_m);
		EXPECT_EQ(off, (offset{1.0_m, 0.0_m, 2.0_m}));
		off -= offset::at_x(1.0_m);
		EXPECT_EQ(off, offset::at_z(2.0_m));
	}

	TEST(gfx_offset, scaling_by_scalar) {
		static_assert(offset{1.0_m, 2.0_m, 3.0_m} * 2 == offset{2.0_m, 4.0_m, 6.0_m});
		static_assert(.5f * offset{1.0_m, 2.0_m, 3.0_m} == offset{0.5_m, 1.0_m, 1.5_m});
		static_assert(offset{1.0_m, 2.0_m, 3.0_m} / 2 == offset{0.5_m, 1.0_m, 1.5_m});
		static_assert(std::is_same_v<decltype(offset{} * 2.0), offset>);
	}

	TEST(gfx_offset, compound_scaling) {
		auto off = offset{1.0_m, 2.0_m, 3.0_m};
		off *= 4;
		EXPECT_EQ(off, (offset{4.0_m, 8.0_m, 12.0_m}));
		off /= 2.0;
		EXPECT_EQ(off, (offset{2.0_m, 4.0_m, 6.0_m}));
	}

	TEST(gfx_offset, glm_round_trip_is_in_metres) {
		EXPECT_EQ(offset(1.0_m, 25_cm, -2.0_m).to_glm(), (glm::dvec3{1.0, 0.25, -2.0}));
		EXPECT_EQ(offset::from(glm::dvec3{1.0, 0.25, -2.0}), (offset{1.0_m, 25_cm, -2.0_m}));
	}

	TEST(gfx_offset, replace_keeps_missing_fields) {
		constexpr offset off{1.0_m, 2.0_m, 3.0_m};
		static_assert(off.replace({}) == off);
		static_assert(off.replace({.y = 5.0_m}) == offset{1.0_m, 5.0_m, 3.0_m});
		static_assert(off.replace({.x = 0.0_m, .z = -1.0_m}) == offset{0.0_m, 2.0_m, -1.0_m});
	}

	TEST(gfx_offset, distances) {
		offset const a{1.0_m, 1.0_m, 1.0_m};
		offset const b{4.0_m, 5.0_m, 1.0_m};
		static_assert(std::is_same_v<decltype(distance(a, b)), length>);
		EXPECT_DOUBLE_EQ(distance(a, b).count(), 5.0);
		EXPECT_DOUBLE_EQ(distance(b, a).count(), 5.0);
		EXPECT_DOUBLE_EQ(distance_squared(a, b).count(), 25.0);
		static_assert(distance_squared(offset{}, offset::at_y(3.0_m)) == 3.0_m * 3.0_m);
	}

	TEST(gfx_offset, lerp_and_midpoint) {
		constexpr offset a{0.0_m, 2.0_m, -4.0_m};
		constexpr offset b{4.0_m, 2.0_m, 4.0_m};
		static_assert(lerp(a, b, 0) == a);
		static_assert(lerp(a, b, 1) == b);
		static_assert(lerp(a, b, .25) == offset{1.0_m, 2.0_m, -2.0_m});
		static_assert(lerp(a, b, 1.5f) == offset{6.0_m, 2.0_m, 8.0_m});
		static_assert(midpoint(a, b) == offset{2.0_m, 2.0_m, 0.0_m});
	}

	TEST(gfx_offset, min_max_are_per_axis) {
		constexpr offset a{1.0_m, -2.0_m, 3.0_m};
		constexpr offset b{-1.0_m, 2.0_m, 3.0_m};
		static_assert(min_pt(a, b) == offset{-1.0_m, -2.0_m, 3.0_m});
		static_assert(max_pt(a, b) == offset{1.0_m, 2.0_m, 3.0_m});
	}

	TEST(gfx_offset, near_checks_every_axis) {
		offset const a{1.0_m, 2.0_m, 3.0_m};
		EXPECT_TRUE(near(a, a + offset::at_z(0.5_um)));
		EXPECT_FALSE(near(a, a + offset::at_z(2_um)));
		EXPECT_TRUE(near(a, a - offset{1_cm, 1_cm, 1_cm}, 2_cm));
		EXPECT_FALSE(near(a, a + offset::at_y(2_cm), 1_cm));
	}

	TEST(gfx_offset, apply_translation) {
		auto const translate = glm::dmat4{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 20, 30, 1};
		EXPECT_EQ(offset(1.0_m, 2.0_m, 3.0_m).apply(translate), (offset{11.0_m, 22.0_m, 33.0_m}));
	}

	TEST(gfx_offset, apply_scale) {
		auto transform = glm::dmat4{2.0};
		transform[3][3] = 1.0;
		EXPECT_EQ(offset(1.0_m, 2.0_m, 3.0_m).apply(transform), (offset{2.0_m, 4.0_m, 6.0_m}));
	}

	TEST(gfx_offset, apply_divides_by_w) {
		auto transform = glm::dmat4{1.0};
		transform[3][3] = 2.0;
		EXPECT_EQ(offset(2.0_m, 4.0_m, 6.0_m).apply(transform), (offset{1.0_m, 2.0_m, 3.0_m}));
	}

	TEST(gfx_offset, apply_direction_ignores_translation) {
		auto transform = glm::dmat4{2.0};
		transform[3] = glm::dvec4{10, 20, 30, 1};
		EXPECT_EQ(offset(1.0_m, 2.0_m, 3.0_m).apply_direction(transform), (offset{2.0_m, 4.0_m, 6.0_m}));
	}

	// ---- size ------------------------------------------------------------

	TEST(gfx_size, cube_has_equal_edges) { static_assert(size::cube(2.0_m) == size{2.0_m, 2.0_m, 2.0_m}); }

	TEST(gfx_size, from_corners) {
		static_assert(size::from_corners(offset{1.0_m, 2.0_m, 3.0_m}, offset{2.0_m, 4.0_m, 6.0_m}) ==
		              size{1.0_m, 2.0_m, 3.0_m});
		static_assert(size::from_corners(offset::at_x(1.0_m), offset{}) == size{-1.0_m, 0.0_m, 0.0_m});
	}

	TEST(gfx_size, replace_keeps_missing_fields) {
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(box.replace({.height = 5.0_m}) == size{1.0_m, 5.0_m, 3.0_m});
	}

	TEST(gfx_size, scaling_by_scalar) {
		static_assert(size{1.0_m, 2.0_m, 3.0_m} * 2 == size{2.0_m, 4.0_m, 6.0_m});
		static_assert(2.0 * size{1.0_m, 2.0_m, 3.0_m} == size{2.0_m, 4.0_m, 6.0_m});
		static_assert(size{1.0_m, 2.0_m, 3.0_m} / 2 == size{0.5_m, 1.0_m, 1.5_m});

		auto box = size::cube(1.0_m);
		box *= 3;
		box /= 2.0f;
		EXPECT_EQ(box, size::cube(1.5_m));
	}

	TEST(gfx_size, scaling_by_optional_factors) {
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(box.scaled({}) == box);
		static_assert(box.scaled({.y = 2}) == size{1.0_m, 4.0_m, 3.0_m});
		static_assert(box.scaled({.x = .5, .z = 2}) == size{0.5_m, 2.0_m, 6.0_m});
	}

	TEST(gfx_size, inflate_adds_deltas_once) {
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(box.inflate({}) == box);
		static_assert(box.inflate({.y = 0.5_m}) == size{1.0_m, 2.5_m, 3.0_m});
		static_assert(box.inflate({.x = 0.25_m, .z = -0.5_m}) == size{1.25_m, 2.0_m, 2.5_m});
		EXPECT_EQ(box.inflate({.x = 1_cm, .y = 1_cm, .z = 1_cm}), (size{1.01_m, 2.01_m, 3.01_m}));
	}

	TEST(gfx_size, arithmetic) {
		static_assert(size{1.0_m, 2.0_m, 3.0_m} + size::cube(1.0_m) == size{2.0_m, 3.0_m, 4.0_m});
		static_assert(size{1.0_m, 2.0_m, 3.0_m} - size::cube(1.0_m) == size{0.0_m, 1.0_m, 2.0_m});

		auto box = size{};
		box += size::cube(2.0_m);
		box -= size{1.0_m, 0.0_m, 0.0_m};
		EXPECT_EQ(box, (size{1.0_m, 2.0_m, 2.0_m}));
	}

	TEST(gfx_size, volume_and_footprint) {
		constexpr size box{2.0_m, 3.0_m, 4.0_m};
		static_assert(std::is_same_v<decltype(box.volume()), math::volume<double>>);
		static_assert(std::is_same_v<decltype(box.footprint()), math::area<double>>);
		EXPECT_DOUBLE_EQ(box.volume().count(), 24.0);
		EXPECT_DOUBLE_EQ(box.footprint().count(), 8.0);
		EXPECT_EQ(size::cube(2.0_m).volume(), 2_m * 2_m * 2_m);
	}

	TEST(gfx_size, adding_to_offset_gives_opposite_corner) {
		constexpr offset corner{1.0_m, 1.0_m, 1.0_m};
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(corner + box == offset{2.0_m, 3.0_m, 4.0_m});
		static_assert(box + corner == offset{2.0_m, 3.0_m, 4.0_m});
		static_assert(size::from_corners(corner, corner + box) == box);
	}

	TEST(gfx_size, axes_map_to_dimensions) {
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(box[axis::x] == box.width);
		static_assert(box[axis::y] == box.height);
		static_assert(box[axis::z] == box.depth);

		auto mutable_box = box;
		mutable_box[axis::y] = 5.0_m;
		EXPECT_EQ(mutable_box.height, 5.0_m);
	}

	TEST(gfx_offset, axes_map_to_coordinates) {
		constexpr offset off{1.0_m, 2.0_m, 3.0_m};
		static_assert(off[axis::x] == off.x);
		static_assert(off[axis::y] == off.y);
		static_assert(off[axis::z] == off.z);
	}

	TEST(gfx_size, center_and_to_offset) {
		constexpr size box{1.0_m, 2.0_m, 3.0_m};
		static_assert(box.to_offset() == offset{1.0_m, 2.0_m, 3.0_m});
		static_assert(box.center() == offset{0.5_m, 1.0_m, 1.5_m});
	}
}  // namespace pyxel::gfx::testing
