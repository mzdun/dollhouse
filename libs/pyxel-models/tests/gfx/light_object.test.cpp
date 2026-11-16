// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/light_object.hpp>
#include <pyxel/gfx/solid_object.hpp>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	namespace {
		light const bulb{.type = light_type::point, .name = "bulb", .intensity = 10.0f};
	}  // namespace

	TEST(gfx_light_object, keeps_a_copy) {
		auto value = bulb;
		light_object const obj{value};
		value.intensity = 1.0f;
		EXPECT_EQ(obj.light(), bulb);
	}

	TEST(gfx_light_object, outputs_light_with_matrix) {
		light_object const obj{bulb};
		auto const matrix = translate(identity_matrix, 1_m, 2_m, 3_m);

		sink out{};
		obj.output(out, matrix);

		auto const& root = out.at(sink::root);
		ASSERT_EQ(root.lights.size(), 1u);
		EXPECT_EQ(root.lights[0].light, bulb);
		EXPECT_EQ(root.lights[0].matrix, matrix);
		// no geometry from a light
		EXPECT_TRUE(root.meshes.empty());
	}

	TEST(gfx_light_object, group_add_wraps_the_light) {
		group grp{};
		grp.add(bulb);
		ASSERT_EQ(grp.children().size(), 1u);
		auto const* wrapped = dynamic_cast<light_object const*>(grp.children()[0].get());
		ASSERT_NE(wrapped, nullptr);
		EXPECT_EQ(wrapped->light(), bulb);
	}

	TEST(gfx_light_object, unnamed_groups_compose_the_matrix) {
		group root{offset::at_x(1_m)};
		root.add<group>(offset::at_y(2_m)).add(bulb);

		sink out{};
		root.output(out, identity_matrix);
		auto const& lights = out.at(sink::root).lights;
		ASSERT_EQ(lights.size(), 1u);
		EXPECT_EQ(lights[0].matrix, translate(identity_matrix, 1_m, 2_m));
	}

	TEST(gfx_light_object, named_group_holds_the_light) {
		group root{};
		auto& lamp = root.add<group>(offset::at_y(2_m));
		lamp.set_name("lamp");
		lamp.add<solid_object>(solid::cube(10_cm, 10_cm, 10_cm));
		lamp.add(bulb);

		sink out{};
		root.output(out, identity_matrix);

		auto const& root_node = out.at(sink::root);
		EXPECT_TRUE(root_node.lights.empty());
		ASSERT_EQ(root_node.children.size(), 1u);
		auto const& node = out.at(root_node.children[0]);
		EXPECT_EQ(node.name, "lamp");
		EXPECT_EQ(node.local, translate(identity_matrix, offset::at_y(2_m)));
		// relative to the node, next to its meshes
		ASSERT_EQ(node.lights.size(), 1u);
		EXPECT_EQ(node.lights[0].matrix, identity_matrix);
		EXPECT_FALSE(node.meshes.empty());
	}

	TEST(gfx_light_object, lights_keep_the_order_of_adding) {
		auto second = bulb;
		second.name = "second";

		group root{};
		root.add(bulb);
		root.add(second);

		sink out{};
		root.output(out, identity_matrix);
		auto const& lights = out.at(sink::root).lights;
		ASSERT_EQ(lights.size(), 2u);
		EXPECT_EQ(lights[0].light.name, "bulb");
		EXPECT_EQ(lights[1].light.name, "second");
	}
}  // namespace pyxel::gfx::testing
