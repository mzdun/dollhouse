// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <array>
#include <pyxel/gfx/interactions.hpp>
#include <vector>

namespace pyxel::gfx::interactions::testing {
	using namespace math::literals;

	TEST(gfx_interactions, channel_single_axis) {
		using changes = std::vector<group_transformation>;
		EXPECT_EQ((changes{channel::translate_y(40_cm), channel::rotate_z(deg(80)), channel::scale_x(2)}),
		          (changes{
		              translate_axis{.on = axis::y, .by = 40_cm},
		              rotate_axis{.around = axis::z, .by = deg(80)},
		              scale_axis{.along = axis::x, .by = 2},
		          }));
	}

	TEST(gfx_interactions, channel_vectors_fill_defaults) {
		EXPECT_EQ(channel::translate(1_m), (translate_all{.by = offset::at_x(1_m)}));
		EXPECT_EQ(channel::translate(offset{1_m, 2_m, 3_m}), channel::translate(1_m, 2_m, 3_m));
		EXPECT_EQ(channel::rotate(deg(5)), (rotate_all{.x = deg(5)}));
		EXPECT_EQ(channel::scale(2), (scale_all{.x = 2, .y = 1, .z = 1}));
		EXPECT_EQ(scale_axis{.along = axis::z}.by, 1);
	}

	TEST(gfx_interactions, state_appends_in_order) {
		std::array<group_transformation const, 2> const more{channel::translate_x(1_cm), channel::scale_y(2)};
		auto const opened = state("opened")
		                        .transform("left", channel::rotate_y(deg(80)))
		                        .transform("right", channel::rotate_y(-deg(80)))
		                        .transform("left", more)
		                        .set("enabled", true)
		                        .set("enabled", false);

		EXPECT_EQ(opened.name, "opened");
		ASSERT_EQ(opened.groups.size(), 2u);
		EXPECT_EQ(opened.groups.at("left"), (std::vector<group_transformation>{
		                                        channel::rotate_y(deg(80)),
		                                        channel::translate_x(1_cm),
		                                        channel::scale_y(2),
		                                    }));
		EXPECT_EQ(opened.groups.at("right"), (std::vector<group_transformation>{channel::rotate_y(-deg(80))}));
		EXPECT_EQ(opened.slots.at("enabled"), (std::vector<slot_transformation>{{.set = true}, {.set = false}}));
	}

	TEST(gfx_interactions, slot_value_keeps_its_type) {
		auto const lit = state("lit").set("level", 0.5).set("on", true).set("tilt", deg(5)).set("depth", 2_cm);
		EXPECT_TRUE(std::holds_alternative<double>(lit.slots.at("level")[0].set));
		EXPECT_TRUE(std::holds_alternative<bool>(lit.slots.at("on")[0].set));
		EXPECT_TRUE(std::holds_alternative<angle>(lit.slots.at("tilt")[0].set));
		EXPECT_TRUE(std::holds_alternative<length>(lit.slots.at("depth")[0].set));
	}

	TEST(gfx_interactions, slot_defaults) {
		slot const enabled{"enabled", slot_type::boolean, direction::output};
		EXPECT_EQ(enabled.name, "enabled");
		EXPECT_EQ(enabled.type, slot_type::boolean);
		EXPECT_EQ(enabled.direction, direction::output);
		EXPECT_FALSE(enabled.default_value);

		slot const dimmer{"level", slot_type::factor, direction::input, 1.0};
		ASSERT_TRUE(dimmer.default_value);
		EXPECT_EQ(*dimmer.default_value, slot_value{1.0});
	}

	TEST(gfx_interactions, action_when_is_a_list) {
		action const tilt{"Tilt", "closed", "tilted"};
		EXPECT_EQ(tilt.when, std::vector<state_name>{"closed"});
		EXPECT_EQ(tilt.then, "tilted");

		action const close{"Close", {"tilted", "opened"}, "closed"};
		EXPECT_EQ(close.when, (std::vector<state_name>{"tilted", "opened"}));
		EXPECT_EQ(close.label, "Close");
	}

	TEST(gfx_interactions, transitions_replace_rules) {
		auto const rules = transitions{}
		                       .for_all("rotate.x 200ms ease-in")
		                       .for_group("tilt-hinge", "rotate.x 100ms")
		                       .for_group("tilt-hinge", "rotate.x 300ms ease-in")
		                       .for_slot("enabled", "0ms linear 45ms");

		EXPECT_EQ(rules.defaults, "rotate.x 200ms ease-in");
		ASSERT_EQ(rules.groups.size(), 1u);
		EXPECT_EQ(rules.groups.at("tilt-hinge"), "rotate.x 300ms ease-in");
		EXPECT_EQ(rules.slots.at("enabled"), "0ms linear 45ms");
	}

	TEST(gfx_interactions, active_objects) {
		active_objects const objects{
		    geo_active_object{
		        .name = "door",
		        .states = {state("closed"), state("opened").transform("door-hinge", channel::rotate_y(deg(80)))},
		        .actions = {{"Open", "closed", "opened"}, {"Close", "opened", "closed"}},
		        .transitions = transitions{}.for_group("door-hinge", "rotate.y 200ms ease-in"),
		    },
		    light_active_object{.name = "lamp", .light = "150ms ease-out"},
		};

		ASSERT_EQ(objects.size(), 2u);
		auto const& door = std::get<geo_active_object>(objects[0]);
		EXPECT_TRUE(door.slots.empty());
		ASSERT_EQ(door.states.size(), 2u);
		EXPECT_EQ(door.states[0].name, "closed");
		EXPECT_EQ(door.actions[1].label, "Close");
		EXPECT_EQ(std::get<light_active_object>(objects[1]).light, "150ms ease-out");
		EXPECT_EQ(objects, active_objects(objects));
	}
}  // namespace pyxel::gfx::interactions::testing
