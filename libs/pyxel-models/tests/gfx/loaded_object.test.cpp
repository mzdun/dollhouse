// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cstddef>
#include <map>
#include <optional>
#include <pyxel/gfx/loaded_object.hpp>
#include <string>
#include <vector>

// The objects of docs/examples/interactions-stored.yaml, loaded into the ones
// of docs/examples/interactions-loaded.yaml.
namespace pyxel::gfx::interactions::testing {
	using namespace math::literals;
	using strings = std::vector<std::string>;
	using edges = std::map<std::ptrdiff_t, size_t>;
	using transformations = std::vector<group_transformation>;

	namespace {
		// the way the parser turns milliseconds into seconds
		constexpr double ms(double value) noexcept { return value * 0.001; }

		constexpr timing ease_in(double duration) noexcept {
			return {.easing = easing::ease_in, .duration = ms(duration)};
		}

		geo_active_object tilt_and_turn() {
			return {
			    .name = "tilt-and-turn",
			    .states = {state("closed"), state("tilted").transform("tilt-hinge", channel::rotate_x(rad(0.0873))),
			               state("opened").transform("right-hinge", channel::rotate_y(rad(1.3963))),
			               state("opened-wide")
			                   .transform("right-hinge", channel::rotate_y(rad(1.3963)))
			                   .transform("left-hinge", channel::rotate_y(rad(-1.3963)))},
			    .actions = {{"Tilt", "closed", "tilted"},
			                {"Open one", "closed", "opened"},
			                {"Open both", "opened", "opened-wide"},
			                {"Close", {"tilted", "opened"}, "closed"},
			                {"Close left", "opened-wide", "opened"}},
			    .transitions = transitions{}
			                       .for_all("rotate.x 200ms ease-in, rotate.y 200ms ease-in")
			                       .for_group("tilt-hinge", "rotate.x 300ms ease-in"),
			};
		}

		geo_active_object door() {
			return {
			    .name = "door",
			    .states = {state("closed"), state("opened").transform("hinge", channel::rotate_y(rad(1.3963)))},
			    .actions = {{"Open door", "closed", "opened"}, {"Close", "opened", "closed"}},
			    .transitions = transitions{}.for_group("hinge", "rotate.y 200ms ease-in"),
			};
		}

		geo_active_object rocker_switch() {
			return {
			    .name = "switch",
			    .slots = {slot{"enabled", slot_type::boolean, direction::output}},
			    .states =
			        {state("switched-off"),
			         state("switched-on").transform("rocker", channel::rotate_x(rad(0.1745))).set("enabled", true)},
			    .actions = {{"Turn on", "switched-off", "switched-on"}, {"Turn off", "switched-on", "switched-off"}},
			    .transitions =
			        transitions{}.for_group("rocker", "rotate.x 50ms ease-in").for_slot("enabled", "0ms linear 45ms"),
			};
		}

		strings highlights(loaded_object const& object, size_t state) {
			strings result{};
			for (auto const group : object.states[state].highlights)
				result.push_back(object.groups[group].group);
			return result;
		}

		strings state_names(loaded_object const& object) {
			strings result{};
			for (auto const& state : object.states)
				result.push_back(state.name);
			return result;
		}
	}  // namespace

	// ---- channels --------------------------------------------------------

	TEST(gfx_loaded_object, channels) {
		EXPECT_EQ(channel_of(channel::translate_z(1_cm)), group_channel::translate_z);
		EXPECT_EQ(channel_of(channel::translate(1_cm)), group_channel::translate);
		EXPECT_EQ(channel_of(channel::rotate_y(deg(5))), group_channel::rotate_y);
		EXPECT_EQ(channel_of(channel::rotate(deg(5))), group_channel::rotate);
		EXPECT_EQ(channel_of(channel::scale_x(2)), group_channel::scale_x);
		EXPECT_EQ(channel_of(channel::scale(2)), group_channel::scale);

		EXPECT_EQ(name_of(group_channel::translate_y), "translate.y");
		EXPECT_EQ(name_of(group_channel::rotate), "rotate");
		EXPECT_EQ(name_of(group_channel::scale_z), "scale.z");

		for (auto const channel : {group_channel::translate_x, group_channel::translate_y, group_channel::translate_z,
		                           group_channel::translate, group_channel::rotate_x, group_channel::rotate_y,
		                           group_channel::rotate_z, group_channel::rotate, group_channel::scale_x,
		                           group_channel::scale_y, group_channel::scale_z, group_channel::scale}) {
			EXPECT_EQ(channel_of(default_of(channel)), channel) << name_of(channel);
		}
		EXPECT_EQ(default_of(group_channel::rotate_x), group_transformation{channel::rotate_x(rad(0))});
		EXPECT_EQ(default_of(group_channel::translate), group_transformation{channel::translate(0_m)});
		EXPECT_EQ(default_of(group_channel::scale_y), group_transformation{channel::scale_y(1)});
		EXPECT_EQ(default_of(group_channel::scale), group_transformation{channel::scale(1)});
	}

	TEST(gfx_loaded_object, slot_defaults) {
		EXPECT_EQ(default_of(slot{"a", slot_type::boolean, direction::output}), slot_value{false});
		EXPECT_EQ(default_of(slot{"a", slot_type::floating, direction::output}), slot_value{0.0});
		EXPECT_EQ(default_of(slot{"a", slot_type::factor, direction::output}), slot_value{0.0});
		EXPECT_EQ(default_of(slot{"a", slot_type::angle, direction::output}), slot_value{angle{}});
		EXPECT_EQ(default_of(slot{"a", slot_type::length, direction::output}), slot_value{length{}});
		EXPECT_EQ(default_of(slot{"a", slot_type::offset, direction::output}), slot_value{offset{}});
		EXPECT_EQ(default_of(slot{"a", slot_type::factor, direction::input, 0.5}), slot_value{0.5});
	}

	// ---- the examples ----------------------------------------------------

	TEST(gfx_loaded_object, tilt_and_turn) {
		auto const object = load(tilt_and_turn());

		EXPECT_EQ(object.name, "tilt-and-turn");
		EXPECT_EQ(object.kind, object_kind::geometry);
		EXPECT_TRUE(object.slots.empty());
		EXPECT_EQ(object.start, 0u);
		EXPECT_EQ(object.actions, (strings{"Tilt", "Open one", "Open both", "Close", "Close left"}));
		ASSERT_EQ(state_names(object), (strings{"closed", "tilted", "opened", "opened-wide"}));

		// machine
		EXPECT_EQ(object.states[0].machine, (edges{{-1, 1}, {0, 1}, {1, 2}}));
		EXPECT_EQ(object.states[1].machine, (edges{{-1, 0}, {3, 0}}));
		EXPECT_EQ(object.states[2].machine, (edges{{-1, 3}, {2, 3}, {3, 0}}));
		EXPECT_EQ(object.states[3].machine, (edges{{-1, 2}, {4, 2}}));

		// state highlights
		EXPECT_EQ(highlights(object, 0), (strings{"tilt-hinge", "right-hinge"}));
		EXPECT_EQ(highlights(object, 1), (strings{"tilt-hinge"}));
		EXPECT_EQ(highlights(object, 2), (strings{"right-hinge", "left-hinge"}));
		EXPECT_EQ(highlights(object, 3), (strings{"left-hinge"}));

		// channels, with transitions: rotate.x of "tilt-hinge" overrides "*"
		EXPECT_EQ(object.groups, (std::vector<group_channels>{
		                             {"tilt-hinge", {group_channel::rotate_x}, {ease_in(300)}},
		                             {"right-hinge", {group_channel::rotate_y}, {ease_in(200)}},
		                             {"left-hinge", {group_channel::rotate_y}, {ease_in(200)}},
		                         }));
		EXPECT_TRUE(object.slot_channels.empty());

		// states
		auto const closed = std::vector<transformations>{
		    {channel::rotate_x(rad(0))},
		    {channel::rotate_y(rad(0))},
		    {channel::rotate_y(rad(0))},
		};
		EXPECT_EQ(object.states[0].values, channel_values{.groups = closed});
		EXPECT_EQ(object.states[1].values.groups, (std::vector<transformations>{
		                                              {channel::rotate_x(rad(0.0873))},
		                                              {channel::rotate_y(rad(0))},
		                                              {channel::rotate_y(rad(0))},
		                                          }));
		EXPECT_EQ(object.states[2].values.groups, (std::vector<transformations>{
		                                              {channel::rotate_x(rad(0))},
		                                              {channel::rotate_y(rad(1.3963))},
		                                              {channel::rotate_y(rad(0))},
		                                          }));
		EXPECT_EQ(object.states[3].values.groups, (std::vector<transformations>{
		                                              {channel::rotate_x(rad(0))},
		                                              {channel::rotate_y(rad(1.3963))},
		                                              {channel::rotate_y(rad(-1.3963))},
		                                          }));

		// storage
		EXPECT_EQ(object.storage, (storage{.state = 0, .progress = 1, .channels = {.groups = closed}}));
		EXPECT_EQ(object.light, timing{});
	}

	TEST(gfx_loaded_object, door) {
		auto const object = load(door());

		EXPECT_EQ(object.actions, (strings{"Open door", "Close"}));
		ASSERT_EQ(state_names(object), (strings{"closed", "opened"}));
		EXPECT_EQ(object.states[0].machine, (edges{{-1, 1}, {0, 1}}));
		EXPECT_EQ(object.states[1].machine, (edges{{-1, 0}, {1, 0}}));
		EXPECT_EQ(highlights(object, 0), (strings{"hinge"}));
		EXPECT_EQ(highlights(object, 1), (strings{"hinge"}));
		EXPECT_EQ(object.groups, (std::vector<group_channels>{{"hinge", {group_channel::rotate_y}, {ease_in(200)}}}));
		EXPECT_EQ(object.states[1].values.groups, (std::vector<transformations>{{channel::rotate_y(rad(1.3963))}}));
		EXPECT_EQ(object.storage, (storage{.state = 0, .channels = {.groups = {{channel::rotate_y(rad(0))}}}}));
	}

	TEST(gfx_loaded_object, rocker_switch) {
		auto const object = load(rocker_switch());

		// the default value is filled in
		ASSERT_EQ(object.slots.size(), 1u);
		EXPECT_EQ(object.slots[0], (slot{"enabled", slot_type::boolean, direction::output, false}));

		EXPECT_EQ(object.states[0].machine, (edges{{-1, 1}, {0, 1}}));
		EXPECT_EQ(object.states[1].machine, (edges{{-1, 0}, {1, 0}}));
		// the slot changes too, but only groups are highlighted
		EXPECT_EQ(highlights(object, 0), (strings{"rocker"}));
		EXPECT_EQ(highlights(object, 1), (strings{"rocker"}));

		EXPECT_EQ(object.groups, (std::vector<group_channels>{{"rocker", {group_channel::rotate_x}, {ease_in(50)}}}));
		// a boolean is never interpolated: it flips once 45 ms have passed
		EXPECT_EQ(object.slot_channels, (std::vector<slot_channel>{
		                                    {.slot = 0, .transition = {.easing = easing::linear, .delay = ms(45)}},
		                                }));

		EXPECT_EQ(object.states[0].values, (channel_values{.groups = {{channel::rotate_x(rad(0))}}, .slots = {false}}));
		EXPECT_EQ(object.states[1].values,
		          (channel_values{.groups = {{channel::rotate_x(rad(0.1745))}}, .slots = {true}}));

		EXPECT_EQ(object.storage,
		          (storage{.state = 0, .channels = {.groups = {{channel::rotate_x(rad(0))}}}, .slots = {false}}));
	}

	TEST(gfx_loaded_object, light_bulb) {
		auto const object = load(light_active_object{.name = "light-bulb", .light = "50ms ease-out"});

		EXPECT_EQ(object.name, "light-bulb");
		EXPECT_EQ(object.kind, object_kind::light);
		EXPECT_EQ(object.slots, (std::vector<slot>{
		                            slot{"enabled", slot_type::boolean, direction::input, true},
		                            slot{"intensity", slot_type::factor, direction::input, 1.0},
		                        }));
		EXPECT_FALSE(object.start);
		EXPECT_TRUE(object.actions.empty());
		EXPECT_TRUE(object.states.empty());
		EXPECT_TRUE(object.groups.empty());
		EXPECT_TRUE(object.slot_channels.empty());
		EXPECT_EQ(object.light, (timing{.easing = easing::ease_out, .duration = ms(50)}));
		EXPECT_EQ(object.storage, (storage{.light = 1.0, .slots = {true, 1.0}}));
	}

	TEST(gfx_loaded_object, every_named_light) {
		active_objects const objects{
		    door(),
		    light_active_object{.name = "light-bulb", .light = "50ms ease-out"},
		};
		// "twice" names two lights; the empty name is no active object
		strings const lights{"lamp", "light-bulb", "", "twice", "twice", "sconce"};
		auto const loaded = load(objects, lights);

		strings names{};
		for (auto const& object : loaded)
			names.push_back(object.name);
		EXPECT_EQ(names, (strings{"door", "light-bulb", "lamp", "sconce"}));
		EXPECT_EQ(loaded[1].light.duration, ms(50));
		// a light no object describes has no transition
		EXPECT_EQ(loaded[2], load(light_active_object{.name = "lamp"}));
		EXPECT_EQ(loaded[2].light, timing{});
		EXPECT_EQ(loaded[2].kind, object_kind::light);
	}

	// ---- details ---------------------------------------------------------

	TEST(gfx_loaded_object, channel_order) {
		auto const object = load(geo_active_object{
		    .name = "drawer",
		    .slots = {slot{"open", slot_type::factor, direction::variable},
		              slot{"seen", slot_type::boolean, direction::input},
		              slot{"depth", slot_type::length, direction::output, length{0.1}}},
		    .states = {state("shut").transform("front", channel::rotate_x(rad(0.1))),
		               // groups new to a state come by name
		               state("out")
		                   .transform("tray", channel::translate_z(length{0.4}))
		                   .transform("front", channel::rotate_x(rad(0.1)))
		                   .transform("front", channel::translate_z(length{0.4}))
		                   .transform("box", channel::scale(2))
		                   .set("depth", length{0.4})
		                   .set("open", 1.0)},
		});

		EXPECT_EQ(object.groups, (std::vector<group_channels>{
		                             {"front", {group_channel::rotate_x, group_channel::translate_z}, {{}, {}}},
		                             {"box", {group_channel::scale}, {{}}},
		                             {"tray", {group_channel::translate_z}, {{}}},
		                         }));
		// in declared order, the inputs left out
		EXPECT_EQ(object.slot_channels, (std::vector<slot_channel>{{.slot = 0}, {.slot = 2}}));

		EXPECT_EQ(object.states[0].values, (channel_values{
		                                       .groups = {{channel::rotate_x(rad(0.1)), channel::translate_z(0_m)},
		                                                  {channel::scale(1)},
		                                                  {channel::translate_z(0_m)}},
		                                       .slots = {0.0, length{0.1}},
		                                   }));
		EXPECT_EQ(object.states[1].values.slots, (std::vector<slot_value>{1.0, length{0.4}}));
		// every slot, in the starting state
		EXPECT_EQ(object.storage.slots, (std::vector<slot_value>{0.0, false, length{0.1}}));
		// no actions: no edges, nothing to highlight
		EXPECT_TRUE(object.states[0].machine.empty());
		EXPECT_TRUE(object.states[0].highlights.empty());
	}

	TEST(gfx_loaded_object, transitions_resolve_per_channel) {
		auto const object = load(geo_active_object{
		    .name = "hatch",
		    .slots = {slot{"open", slot_type::boolean, direction::output}},
		    .states = {state("shut"), state("open")
		                                  .transform("lid", channel::rotate_x(rad(1)))
		                                  .transform("lid", channel::translate_y(length{0.1}))
		                                  .transform("vent", channel::scale_y(0))
		                                  .set("open", true)},
		    .transitions = transitions{}  // "set" is skipped for the groups, "rotate.x" for the slot
		                       .for_all("all 1s, rotate.x 2s linear, set 3s")
		                       // the later item wins, "all" included
		                       .for_group("lid", "rotate.x 4s, all 5s ease-out")
		                       // a value that does not parse adds nothing to "*"
		                       .for_group("vent", "1s 2s 3s"),
		});

		EXPECT_EQ(object.groups,
		          (std::vector<group_channels>{
		              {"lid",
		               {group_channel::rotate_x, group_channel::translate_y},
		               {{.easing = easing::ease_out, .duration = 5}, {.easing = easing::ease_out, .duration = 5}}},
		              {"vent", {group_channel::scale_y}, {{.duration = 1}}},
		          }));
		EXPECT_EQ(object.slot_channels, (std::vector<slot_channel>{{.slot = 0, .transition = {.duration = 3}}}));
	}

	TEST(gfx_loaded_object, broken_declarations) {
		auto const object = load(geo_active_object{
		    .name = "broken",
		    .states = {state("a"), state("b").transform("g", channel::rotate_y(rad(1))), state("a").set("x", true)},
		    .actions = {{"To b", "a", "b"}, {"Nowhere", "a", "c"}, {"From nowhere", "c", "a"}, {"Stay", "b", "b"}},
		    .transitions = transitions{}.for_slot("x", "1s"),
		});

		// the first declaration of "a" wins; "x" is not declared
		EXPECT_EQ(state_names(object), (strings{"a", "b"}));
		EXPECT_TRUE(object.slot_channels.empty());
		EXPECT_EQ(object.actions, (strings{"To b", "Nowhere", "From nowhere", "Stay"}));
		EXPECT_EQ(object.states[0].machine, (edges{{-1, 1}, {0, 1}}));
		// a loop moves nothing
		EXPECT_EQ(object.states[1].machine, (edges{{-1, 1}, {3, 1}}));
		EXPECT_EQ(highlights(object, 0), (strings{"g"}));
		EXPECT_TRUE(object.states[1].highlights.empty());
		EXPECT_EQ(object.find_state("b"), 1u);
		EXPECT_FALSE(object.find_state("c"));
	}

	TEST(gfx_loaded_object, no_states) {
		auto const object = load(geo_active_object{.name = "empty", .transitions = transitions{}.for_all("1s")});
		EXPECT_FALSE(object.start);
		EXPECT_TRUE(object.states.empty());
		EXPECT_EQ(object.storage, storage{});
	}
}  // namespace pyxel::gfx::interactions::testing
