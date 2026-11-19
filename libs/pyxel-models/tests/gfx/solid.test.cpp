// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/solid.hpp>
#include <type_traits>
#include <utility>
#include <vector>

namespace pyxel::gfx::testing {
	using namespace math::literals;
	using faces_t = std::vector<std::vector<size_t>>;

	TEST(gfx_solid, default_is_empty) {
		solid const obj{};
		EXPECT_TRUE(obj.vertices().empty());
		EXPECT_TRUE(obj.faces().empty());
	}

	TEST(gfx_solid, construct_from_copies) {
		std::vector<offset> const vertices{offset{}, offset::at_x(1.0_m), offset::at_y(1.0_m)};
		faces_t const faces{{0, 1, 2}};
		solid const obj{vertices, faces};
		EXPECT_EQ(obj.vertices(), vertices);
		EXPECT_EQ(obj.faces(), faces);
	}

	TEST(gfx_solid, construct_from_temporaries) {
		solid const obj{std::vector<offset>{offset{}, offset::at_x(1.0_m), offset::at_y(1.0_m)}, faces_t{{0, 1, 2}}};
		EXPECT_EQ(obj.vertices(), (std::vector<offset>{offset{}, offset::at_x(1.0_m), offset::at_y(1.0_m)}));
		EXPECT_EQ(obj.faces(), (faces_t{{0, 1, 2}}));
	}

	TEST(gfx_solid, reserve_keeps_contents_empty) {
		solid obj{};
		obj.reserve(8, 6);
		EXPECT_TRUE(obj.vertices().empty());
		EXPECT_TRUE(obj.faces().empty());
		EXPECT_GE(obj.vertices().capacity(), 8u);
		EXPECT_GE(obj.faces().capacity(), 6u);
	}

	TEST(gfx_solid, add_vert_returns_consecutive_indices) {
		solid obj{};
		EXPECT_EQ(obj.add_vert(offset{}), 0u);
		EXPECT_EQ(obj.add_vert(1.0_m, 2.0_m, 3.0_m), 1u);
		EXPECT_EQ(obj.add_vert(offset::at_z(25_cm)), 2u);
		EXPECT_EQ(obj.vertices(), (std::vector<offset>{offset{}, offset{1.0_m, 2.0_m, 3.0_m}, offset::at_z(25_cm)}));
	}

	TEST(gfx_solid, add_vert_reuses_equal_vertices) {
		solid obj{};
		EXPECT_EQ(obj.add_vert(offset{}), 0u);
		EXPECT_EQ(obj.add_vert(offset::at_x(1.0_m)), 1u);
		EXPECT_EQ(obj.add_vert(offset{}), 0u);
		EXPECT_EQ(obj.add_vert(1.0_m, 0.0_m, 0.0_m), 1u);
		EXPECT_EQ(obj.vertices(), (std::vector<offset>{offset{}, offset::at_x(1.0_m)}));
	}

	TEST(gfx_solid, add_vert_compares_across_units) {
		solid obj{};
		EXPECT_EQ(obj.add_vert(offset::at_x(1.0_m)), 0u);
		EXPECT_EQ(obj.add_vert(offset::at_x(100_cm)), 0u);
		EXPECT_EQ(obj.add_vert(offset::at_x(1000_mm)), 0u);
		EXPECT_EQ(obj.vertices().size(), 1u);
	}

	TEST(gfx_solid, add_vert_keeps_close_vertices_apart) {
		solid obj{};
		EXPECT_EQ(obj.add_vert(offset::at_x(1.0_m)), 0u);
		EXPECT_EQ(obj.add_vert(offset::at_x(1.0_m) + offset::at_x(1_um)), 1u);
		EXPECT_EQ(obj.vertices().size(), 2u);
	}

	TEST(gfx_solid, add_vert_finds_constructed_vertices) {
		solid obj{std::vector<offset>{offset{}, offset::at_x(1.0_m), offset::at_y(1.0_m)}, faces_t{{0, 1, 2}}};
		EXPECT_EQ(obj.add_vert(offset::at_y(1.0_m)), 2u);
		EXPECT_EQ(obj.add_vert(offset::at_z(1.0_m)), 3u);
		EXPECT_EQ(obj.vertices().size(), 4u);
	}

	TEST(gfx_solid, constructor_keeps_duplicates_as_given) {
		// faces refer to the given indices, so they are not merged here
		solid obj{std::vector<offset>{offset{}, offset::at_x(1.0_m), offset{}}, faces_t{{0, 1, 2}}};
		EXPECT_EQ(obj.vertices().size(), 3u);
		EXPECT_EQ(obj.faces(), (faces_t{{0, 1, 2}}));
		// the first of equal vertices is reused
		EXPECT_EQ(obj.add_vert(offset{}), 0u);
	}

	TEST(gfx_solid, copies_deduplicate_independently) {
		solid obj{};
		obj.add_vert(offset{});
		auto copy = obj;
		EXPECT_EQ(copy.add_vert(offset::at_x(1.0_m)), 1u);
		EXPECT_EQ(obj.add_vert(offset::at_y(1.0_m)), 1u);
		EXPECT_EQ(copy.add_vert(offset{}), 0u);
		EXPECT_EQ(obj.add_vert(offset::at_x(1.0_m)), 2u);
	}

	TEST(gfx_solid, add_face_from_indices) {
		solid obj{};
		obj.add_face(size_t{0}, size_t{1}, size_t{2});
		obj.add_face(size_t{0}, size_t{2}, size_t{3}, size_t{4});
		EXPECT_EQ(obj.faces(), (faces_t{{0, 1, 2}, {0, 2, 3, 4}}));
	}

	TEST(gfx_solid, add_face_from_vector) {
		solid obj{};
		obj.add_face(std::vector<size_t>{3, 2, 1});
		EXPECT_EQ(obj.faces(), (faces_t{{3, 2, 1}}));
	}

	TEST(gfx_solid, empty_is_valid) { EXPECT_TRUE(solid{}.is_valid()); }

	TEST(gfx_solid, faces_with_known_vertices_are_valid) {
		solid obj{};
		auto const a = obj.add_vert(offset{});
		auto const b = obj.add_vert(offset::at_x(1.0_m));
		auto const c = obj.add_vert(offset::at_y(1.0_m));
		obj.add_face(a, b, c);
		EXPECT_TRUE(obj.is_valid());
	}

	TEST(gfx_solid, faces_before_vertices_become_valid) {
		solid obj{};
		obj.add_face(size_t{0}, size_t{1}, size_t{2});
		EXPECT_FALSE(obj.is_valid());
		obj.add_vert(offset{});
		obj.add_vert(offset::at_x(1.0_m));
		EXPECT_FALSE(obj.is_valid());
		obj.add_vert(offset::at_y(1.0_m));
		EXPECT_TRUE(obj.is_valid());
	}

	TEST(gfx_solid, duplicate_vertex_does_not_make_face_valid) {
		solid obj{};
		obj.add_vert(offset{});
		obj.add_vert(offset::at_x(1.0_m));
		obj.add_face(size_t{0}, size_t{1}, size_t{2});
		obj.add_vert(offset{});  // reused, nothing appended
		EXPECT_FALSE(obj.is_valid());
	}

	TEST(gfx_solid, one_bad_face_makes_solid_invalid) {
		solid const obj{std::vector<offset>{offset{}, offset::at_x(1.0_m), offset::at_y(1.0_m)},
		                faces_t{{0, 1, 2}, {2, 1, 3}}};
		EXPECT_FALSE(obj.is_valid());
	}

	TEST(gfx_solid, short_faces_are_not_checked_for_shape) {
		// only the indices are checked, not the face sizes
		solid const obj{std::vector<offset>{offset{}}, faces_t{{}, {0}, {0, 0}}};
		EXPECT_TRUE(obj.is_valid());
	}

	TEST(gfx_solid, accessors_are_read_only) {
		static_assert(std::is_same_v<decltype(std::declval<solid&>().vertices()), std::vector<offset> const&>);
		static_assert(std::is_same_v<decltype(std::declval<solid&>().faces()), faces_t const&>);
	}
}  // namespace pyxel::gfx::testing
