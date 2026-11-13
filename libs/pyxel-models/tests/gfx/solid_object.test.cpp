// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/gfx/transform.hpp>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	TEST(gfx_solid_object, keeps_a_copy) {
		auto shape = solid::cube(1_m, 1_m, 1_m);
		solid_object const obj{shape};
		shape.add_vert(offset::at_x(5_m));
		EXPECT_EQ(obj.shape().vertices().size(), 8u);
	}

	TEST(gfx_solid_object, outputs_through_the_group) {
		group root{offset::at_x(10_m)};
		root.add<solid_object>(solid::cube(1_m, 1_m, 1_m));

		sink out{};
		root.output(out, identity_matrix);
		ASSERT_EQ(out.solids().size(), 1u);
		auto const& [material, by_name] = *out.solids().begin();
		ASSERT_EQ(by_name.size(), 1u);
		auto const& added = by_name.begin()->second;
		EXPECT_EQ(added.vertices().size(), 8u);
		EXPECT_EQ(added.faces().size(), 6u);
		for (auto const& vertex : added.vertices()) {
			EXPECT_GE(vertex.x, 10.0);
			EXPECT_LE(vertex.x, 11.0);
		}
	}
}  // namespace pyxel::gfx::testing
