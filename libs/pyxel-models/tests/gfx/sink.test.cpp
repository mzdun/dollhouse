// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/sink.hpp>

namespace pyxel::gfx::testing {
	using namespace math::literals;
	using faces_t = sink::glm_solid::faces_t;
	using points_t = std::vector<glm::dvec3>;
	using strings = sink::strings;

	namespace {
		glm::dmat4 const identity{1.0};

		glm::dmat4 translation(double x, double y, double z) {
			glm::dmat4 result{1.0};
			result[3] = glm::dvec4{x, y, z, 1.0};
			return result;
		}

		glm::dmat4 mirror_x() {
			glm::dmat4 result{1.0};
			result[0][0] = -1.0;
			return result;
		}

		// unit square in the x-y plane, counter-clockwise
		solid square() {
			solid obj{};
			auto const a = obj.add_vert(0.0_m, 0.0_m, 0.0_m);
			auto const b = obj.add_vert(1.0_m, 0.0_m, 0.0_m);
			auto const c = obj.add_vert(1.0_m, 1.0_m, 0.0_m);
			auto const d = obj.add_vert(0.0_m, 1.0_m, 0.0_m);
			obj.add_face(a, b, c, d);
			return obj;
		}
	}  // namespace

	// ---- sink::glm_solid -------------------------------------------------

	TEST(gfx_glm_solid, default_is_empty) {
		sink::glm_solid const out{};
		EXPECT_TRUE(out.vertices().empty());
		EXPECT_TRUE(out.faces().empty());
	}

	TEST(gfx_glm_solid, identity_copies_solid) {
		sink::glm_solid out{};
		out.add_solid(square(), identity);
		EXPECT_EQ(out.vertices(), (points_t{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}));
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2, 3}}));
	}

	TEST(gfx_glm_solid, vertices_are_transformed) {
		sink::glm_solid out{};
		out.add_solid(square(), translation(2, 3, 4));
		EXPECT_EQ(out.vertices(), (points_t{{2, 3, 4}, {3, 3, 4}, {3, 4, 4}, {2, 4, 4}}));
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2, 3}}));
	}

	TEST(gfx_glm_solid, vertices_are_in_metres) {
		solid obj{};
		obj.add_vert(25_cm, 5_mm, -1.0_m);
		obj.add_face(size_t{0}, size_t{0}, size_t{0});
		sink::glm_solid out{};
		out.add_solid(obj, identity);
		ASSERT_EQ(out.vertices().size(), 1u);
		EXPECT_DOUBLE_EQ(out.vertices()[0].x, 0.25);
		EXPECT_DOUBLE_EQ(out.vertices()[0].y, 0.005);
		EXPECT_DOUBLE_EQ(out.vertices()[0].z, -1.0);
	}

	TEST(gfx_glm_solid, shared_vertices_are_merged_across_solids) {
		sink::glm_solid out{};
		out.add_solid(square(), identity);
		out.add_solid(square(), translation(1, 0, 0));
		EXPECT_EQ(out.vertices(), (points_t{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {2, 0, 0}, {2, 1, 0}}));
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2, 3}, {1, 4, 5, 2}}));
	}

	// the constructor keeps duplicate vertices, add_vert would merge them
	// before they reach the sink

	TEST(gfx_glm_solid, duplicate_vertices_are_merged_within_solid) {
		solid const obj{
		    std::vector<offset>{
		        offset{},
		        offset::at_x(1.0_m),
		        offset::at_y(1.0_m),
		        offset::at_x(1.0_m),  // same as 1
		        offset::at_z(1.0_m),
		    },
		    faces_t{{0, 1, 2}, {0, 3, 4}},
		};
		sink::glm_solid out{};
		out.add_solid(obj, identity);
		EXPECT_EQ(out.vertices(), (points_t{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}}));
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2}, {0, 1, 3}}));
	}

	TEST(gfx_glm_solid, repeated_corners_are_collapsed) {
		solid const obj{
		    std::vector<offset>{
		        offset{}, offset::at_x(1.0_m),
		        offset::at_x(1.0_m),            // same as 1
		        offset::at_y(1.0_m), offset{},  // same as 0
		    },
		    faces_t{{0, 1, 2, 3, 4}},
		};
		sink::glm_solid out{};
		out.add_solid(obj, identity);
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2}}));
	}

	TEST(gfx_glm_solid, vertices_merged_by_transform_are_collapsed) {
		// distinct in the solid, but flattened onto each other by the matrix
		solid obj{};
		auto const a = obj.add_vert(0.0_m, 0.0_m, 0.0_m);
		auto const b = obj.add_vert(1.0_m, 0.0_m, 0.0_m);
		auto const c = obj.add_vert(1.0_m, 0.0_m, 1.0_m);
		auto const d = obj.add_vert(0.0_m, 1.0_m, 0.0_m);
		obj.add_face(a, b, c, d);
		obj.add_face(a, b, c);

		glm::dmat4 flatten_z{1.0};
		flatten_z[2][2] = 0.0;

		sink::glm_solid out{};
		out.add_solid(obj, flatten_z);
		EXPECT_EQ(out.vertices(), (points_t{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}));
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2}}));
	}

	TEST(gfx_glm_solid, degenerate_faces_are_dropped) {
		solid obj{
		    std::vector<offset>{
		        offset{}, offset::at_x(1.0_m),
		        offset::at_x(1.0_m),  // same as 1
		    },
		    faces_t{{0, 1, 2}, {0, 1}},
		};
		obj.add_face(std::vector<size_t>{});
		sink::glm_solid out{};
		out.add_solid(obj, identity);
		EXPECT_EQ(out.vertices().size(), 2u);
		EXPECT_TRUE(out.faces().empty());
	}

	TEST(gfx_glm_solid, faces_with_bad_indices_are_dropped) {
		auto obj = square();
		obj.add_face(size_t{0}, size_t{1}, size_t{4});
		sink::glm_solid out{};
		out.add_solid(obj, identity);
		EXPECT_EQ(out.faces(), (faces_t{{0, 1, 2, 3}}));
	}

	TEST(gfx_glm_solid, mirroring_reverses_winding) {
		sink::glm_solid out{};
		out.add_solid(square(), mirror_x());
		EXPECT_EQ(out.vertices(), (points_t{{0, 0, 0}, {-1, 0, 0}, {-1, 1, 0}, {0, 1, 0}}));
		EXPECT_EQ(out.faces(), (faces_t{{3, 2, 1, 0}}));
	}

	TEST(gfx_glm_solid, mutable_faces_accessor) {
		sink::glm_solid out{};
		out.add_solid(square(), identity);
		out.faces().clear();
		EXPECT_TRUE(out.faces().empty());
		EXPECT_EQ(out.vertices().size(), 4u);
	}

	// ---- sink ------------------------------------------------------------

	TEST(gfx_sink, default_is_empty) {
		sink const out{};
		EXPECT_TRUE(out.solids().empty());
	}

	TEST(gfx_sink, solids_without_context_go_to_empty_keys) {
		sink out{};
		out.add_solid(square(), identity);
		ASSERT_EQ(out.solids().size(), 1u);
		auto const& by_name = out.solids().at("");
		ASSERT_EQ(by_name.size(), 1u);
		EXPECT_EQ(by_name.at(strings{}).faces().size(), 1u);
	}

	TEST(gfx_sink, solids_are_grouped_by_current_material) {
		sink out{};
		out.push_material("wood");
		out.add_solid(square(), identity);
		out.push_material("glass");
		out.add_solid(square(), translation(0, 0, 1));
		out.pop_material();
		out.add_solid(square(), translation(0, 0, 2));
		out.pop_material();
		out.add_solid(square(), translation(0, 0, 3));

		auto const& solids = out.solids();
		ASSERT_EQ(solids.size(), 3u);
		EXPECT_EQ(solids.at("wood").at(strings{}).faces().size(), 2u);
		EXPECT_EQ(solids.at("glass").at(strings{}).faces().size(), 1u);
		EXPECT_EQ(solids.at("").at(strings{}).faces().size(), 1u);
	}

	TEST(gfx_sink, solids_are_grouped_by_full_name_stack) {
		sink out{};
		out.push_object_name("house");
		out.add_solid(square(), identity);
		out.push_object_name("door");
		out.add_solid(square(), translation(0, 0, 1));
		out.pop_object_name();
		out.push_object_name("window");
		out.add_solid(square(), translation(0, 0, 2));
		out.pop_object_name();
		out.pop_object_name();

		auto const& by_name = out.solids().at("");
		ASSERT_EQ(by_name.size(), 3u);
		EXPECT_EQ(by_name.at(strings{"house"}).faces().size(), 1u);
		EXPECT_EQ(by_name.at((strings{"house", "door"})).faces().size(), 1u);
		EXPECT_EQ(by_name.at((strings{"house", "window"})).faces().size(), 1u);
	}

	TEST(gfx_sink, material_and_name_are_independent) {
		sink out{};
		out.push_object_name("door");
		out.push_material("wood");
		out.add_solid(square(), identity);
		out.pop_object_name();
		out.add_solid(square(), translation(0, 0, 1));

		auto const& by_name = out.solids().at("wood");
		ASSERT_EQ(by_name.size(), 2u);
		EXPECT_EQ(by_name.at(strings{"door"}).faces().size(), 1u);
		EXPECT_EQ(by_name.at(strings{}).faces().size(), 1u);
	}

	TEST(gfx_sink, solids_in_same_group_share_vertices) {
		sink out{};
		out.add_solid(square(), identity);
		out.add_solid(square(), translation(1, 0, 0));
		auto const& group = out.solids().at("").at(strings{});
		EXPECT_EQ(group.vertices().size(), 6u);
		EXPECT_EQ(group.faces().size(), 2u);
	}

	TEST(gfx_sink, popping_empty_stacks_is_harmless) {
		sink out{};
		out.pop_material();
		out.pop_object_name();
		out.add_solid(square(), identity);
		EXPECT_EQ(out.solids().at("").at(strings{}).faces().size(), 1u);
	}

	TEST(gfx_sink, mutable_solids_accessor) {
		sink out{};
		out.add_solid(square(), identity);
		out.solids().clear();
		EXPECT_TRUE(out.solids().empty());
	}
}  // namespace pyxel::gfx::testing
