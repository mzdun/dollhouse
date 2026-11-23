// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "interactions_rules.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <limits>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/io/export.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::io::testing {
	using namespace gfx::interactions;
	using gfx::length;
	using gfx::rad;
	using io::interactions::check;
	using io::interactions::easing;
	using io::interactions::model_names;
	using io::interactions::parse_transition;
	using io::interactions::problem;
	using io::interactions::single_transition;
	using strings = std::vector<std::string>;

	namespace {
		model_names model() {
			model_names result{};
			for (auto const* name : {"frame", "hinge", "tilt-hinge", "twice", "twice", "lamp", ""})
				result.add_node(name);
			result.add_light("lamp");
			// lights with no name are not active objects, and share no name
			result.add_light("");
			result.add_light("");
			return result;
		}

		geo_active_object window() {
			return {
			    .name = "window",
			    .slots = {slot{"open", slot_type::boolean, direction::output},
			              slot{"level", slot_type::factor, direction::input, 1.0}},
			    .states = {state("closed").set("open", false),
			               state("tilted").transform("tilt-hinge", channel::rotate_x(rad(0.1))),
			               state("opened")
			                   .transform("hinge", channel::translate_z(length{0.01}))
			                   .transform("hinge", channel::rotate_y(rad(1.4)))
			                   .set("open", true)},
			    .actions = {{"Tilt", "closed", "tilted"},
			                {"Open", "closed", "opened"},
			                {"Close", {"tilted", "opened"}, "closed"}},
			    .transitions = transitions{}
			                       .for_all("rotate.x 200ms ease-in, rotate.y 200ms ease-in, set 1s")
			                       .for_group("tilt-hinge", "rotate.x 300ms ease-in")
			                       .for_slot("open", "0ms linear 45ms"),
			};
		}

		// the reasons given for the first object
		strings reasons(active_objects const& objects, model_names const& names = model()) {
			strings result{};
			for (auto const& [object, reason] : check(objects, names)) {
				if (object == 0u) result.push_back(reason);
			}
			return result;
		}

		strings reasons(geo_active_object const& object) { return reasons(active_objects{object}); }

		void expect_items(std::string_view value, std::vector<single_transition> const& expected) {
			auto const result = parse_transition(value);
			EXPECT_EQ(result.error, "") << value;
			ASSERT_EQ(result.items.size(), expected.size()) << value;
			for (size_t index = 0; index < expected.size(); ++index) {
				EXPECT_EQ(result.items[index].channel, expected[index].channel) << value;
				EXPECT_EQ(result.items[index].easing, expected[index].easing) << value;
				EXPECT_DOUBLE_EQ(result.items[index].duration, expected[index].duration) << value;
				EXPECT_DOUBLE_EQ(result.items[index].delay, expected[index].delay) << value;
			}
		}

		std::string error_of(std::string_view value) {
			auto const result = parse_transition(value);
			EXPECT_TRUE(result.items.empty()) << value;
			return result.error;
		}

		constexpr auto infinity = std::numeric_limits<double>::infinity();
	}  // namespace

	// ---- objects ---------------------------------------------------------

	TEST(interactions_rules, valid_objects_pass) {
		EXPECT_EQ(check({window(), light_active_object{.name = "lamp", .light = "150ms"}}, model()),
		          std::vector<problem>{});
		EXPECT_EQ(check({light_active_object{.name = "lamp"}}, model()), std::vector<problem>{});
		EXPECT_EQ(check({}, model()), std::vector<problem>{});
	}

	TEST(interactions_rules, object_names) {
		auto object = window();
		object.name = "a.b";
		EXPECT_EQ(reasons(object), strings{"has a name with a `.` or a `/` in it"});
		object.name = "a/b";
		EXPECT_EQ(reasons(object), strings{"has a name with a `.` or a `/` in it"});
		object.name = "";
		EXPECT_EQ(reasons(object), strings{"has no name"});
		object.name = "lamp";
		EXPECT_EQ(reasons(object), strings{"takes the name of a light, but is not an object describing it"});
	}

	TEST(interactions_rules, lights) {
		EXPECT_EQ(reasons({light_active_object{.name = "nothing"}}), strings{"describes no light of the model"});
		EXPECT_EQ(reasons({light_active_object{.name = "lamp", .light = "rotate.x 1s"}}),
		          strings{"has the transition of the light, which names channel `rotate.x` it does not have"});
		EXPECT_EQ(reasons({light_active_object{.name = "lamp", .light = "fast"}}),
		          strings{"has the transition of the light, which has `fast`, which is not a channel, a timing "
		                  "function or a time"});
		EXPECT_EQ(reasons({light_active_object{.name = "lamp", .light = "set 1s"}}), strings{});

		auto const twice = check({light_active_object{.name = "lamp"}, light_active_object{.name = "lamp"}}, model());
		EXPECT_EQ(twice, (std::vector<problem>{
		                     {.object = 0, .reason = "describes a light, which another object describes"},
		                     {.object = 1, .reason = "describes a light, which another object describes"},
		                 }));
	}

	TEST(interactions_rules, lights_of_the_model) {
		model_names names{};
		names.add_light("x");
		names.add_light("x");
		names.add_light("a.b");

		EXPECT_EQ(check({light_active_object{.name = "x"}}, names),
		          (std::vector<problem>{
		              {.reason = "light `a.b` has a `.` or a `/` in its name"},
		              {.reason = "2 lights share the name `x`"},
		              {.object = 0, .reason = "describes a light, whose name is not unique"},
		          }));
	}

	TEST(interactions_rules, slots) {
		auto object = window();
		object.slots.push_back(slot{"open", slot_type::floating, direction::input});
		object.slots.push_back(slot{"visible", slot_type::boolean, direction::input});
		object.slots.push_back(slot{"a/b", slot_type::boolean, direction::input});
		object.slots.push_back(slot{"tilt", slot_type::angle, direction::input, true});
		object.slots.push_back(slot{"dim", slot_type::factor, direction::input, 1.5});
		object.slots.push_back(slot{"speed", slot_type::floating, direction::input, infinity});
		object.slots.push_back(slot{"at", slot_type::offset, direction::input, gfx::offset{}});

		EXPECT_EQ(reasons(object), (strings{
		                               "has slot `open` declared twice",
		                               "has slot `visible`, which is reserved",
		                               "has slot `a/b`, with a `.` or a `/` in its name",
		                               "has slot `tilt` with a default, which its type does not accept",
		                               "has slot `dim` with a default, which its type does not accept",
		                               "has slot `speed` with a default, which its type does not accept",
		                           }));
	}

	TEST(interactions_rules, groups) {
		auto object = window();
		object.states.push_back(state("s")
		                            .transform("nowhere", channel::scale_x(2))
		                            .transform("twice", channel::scale_x(2))
		                            .transform("$g", channel::scale_x(2))
		                            .transform("lamp", channel::scale_x(2)));

		auto actual = reasons(object);
		std::ranges::sort(actual);
		EXPECT_EQ(actual, (strings{
		                      "names group `$g` in state `s`, which starts with a `$`",
		                      "names group `lamp` in state `s`, which is the name of a light",
		                      "names group `nowhere` in state `s`, which is not a node of the model",
		                      "names group `twice` in state `s`, which is the name of 2 nodes",
		                  }));
	}

	TEST(interactions_rules, group_channels) {
		auto twice = window();
		twice.states.push_back(
		    state("s").transform("hinge", channel::rotate_y(rad(1))).transform("hinge", channel::rotate_y(rad(2))));
		EXPECT_EQ(reasons(twice), strings{"sets `rotate.y` of group `hinge` in state `s` twice"});

		auto infinite = window();
		infinite.states.push_back(state("s").transform("hinge", channel::translate(length{infinity})));
		EXPECT_EQ(reasons(infinite),
		          strings{"sets `translate` of group `hinge` in state `s` to a value, which is not finite"});

		// "opened" lists translate.z before rotate.y
		auto reordered = window();
		reordered.states.push_back(state("other")
		                               .transform("hinge", channel::rotate_y(rad(1)))
		                               .transform("hinge", channel::translate_z(length{1})));
		EXPECT_EQ(reasons(reordered),
		          strings{"lists the channels of group `hinge` in state `other` in another order than before"});

		// a channel no earlier state names comes after all the ones they do,
		// so `scale` cannot come before `rotate.y`
		auto extended = window();
		extended.states.push_back(state("other")
		                              .transform("hinge", channel::translate_z(length{1}))
		                              .transform("hinge", channel::scale(2))
		                              .transform("hinge", channel::rotate_y(rad(1))));
		EXPECT_EQ(reasons(extended),
		          strings{"lists the channels of group `hinge` in state `other` in another order than before"});

		auto subset = window();
		subset.states.push_back(state("other").transform("hinge", channel::rotate_y(rad(1))));
		EXPECT_EQ(reasons(subset), strings{});
	}

	TEST(interactions_rules, slots_in_states) {
		auto object = window();
		object.states.push_back(state("s")
		                            .set("nope", true)
		                            .set("level", 0.5)
		                            .set("open", true)
		                            .set("open", false)
		                            .transform("hinge", channel::scale_x(2)));
		object.states.push_back(state("t").set("open", 1.0));

		auto actual = reasons(object);
		std::ranges::sort(actual);
		EXPECT_EQ(actual, (strings{
		                      "names slot `level` in state `s`, which is an input",
		                      "names slot `nope` in state `s`, which it does not declare",
		                      "sets slot `open` in state `s` twice",
		                      "sets slot `open` in state `t` to a value, which its type does not accept",
		                  }));
	}

	TEST(interactions_rules, states_and_actions) {
		auto object = window();
		object.states.push_back(state("closed"));
		object.actions.emplace_back("Go", "nowhere", "closed");
		object.actions.emplace_back("Lost", "closed", "lost");
		object.actions.emplace_back("Empty", std::vector<state_name>{}, "closed");
		object.actions.emplace_back("", "closed", "lost");

		EXPECT_EQ(reasons(object), (strings{
		                               "declares state `closed` twice",
		                               "has action `Go` leaving state `nowhere`, which it does not declare",
		                               "has action `Lost` leading to state `lost`, which it does not declare",
		                               "has action `Empty` leaving no state",
		                               "has action 7 with an empty `label`",
		                               "has action 7 leading to state `lost`, which it does not declare",
		                           }));
	}

	TEST(interactions_rules, transitions) {
		auto targets = window();
		targets.transitions.for_group("nowhere", "1s").for_slot("level", "1s").for_slot("nope", "1s");
		auto actual = reasons(targets);
		std::ranges::sort(actual);
		EXPECT_EQ(actual, (strings{
		                      "names group `nowhere` in `transitions`, which is not a node of the model",
		                      "names slot `level` in `transitions`, which is an input",
		                      "names slot `nope` in `transitions`, which it does not declare",
		                  }));

		auto channels = window();
		channels.transitions.for_group("hinge", "set 1s").for_slot("open", "rotate.x 1s");
		actual = reasons(channels);
		std::ranges::sort(actual);
		EXPECT_EQ(actual, (strings{
		                      "has a transition of group `hinge`, which names channel `set` it does not have",
		                      "has a transition of slot `open`, which names channel `rotate.x` it does not have",
		                  }));

		auto values = window();
		values.transitions.for_all("fast").for_group("hinge", "1");
		EXPECT_EQ(reasons(values), (strings{
		                               "has a default transition, which has `fast`, which is not a channel, a timing "
		                               "function or a time",
		                               "has a transition of group `hinge`, which has `1`, which is not a channel, a "
		                               "timing function or a time",
		                           }));
	}

	TEST(interactions_rules, export_fails_on_problems) {
		gfx::group root{};
		auto& frame = root.add_group({.name = "frame"});
		frame.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(10), gfx::cm(10), gfx::cm(10)));
		frame.add_active_object(geo_active_object{
		    .name = "door",
		    .states = {state("closed"), state("opened").transform("nowhere", channel::rotate_y(rad(1)))},
		});
		frame.set_light_transition("lamp", "1s");

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});

		auto const path = std::filesystem::temp_directory_path() / "pyxel-io-test" / "rules-door.glb";
		std::filesystem::create_directories(path.parent_path());
		try {
			export_scene("bad", out, gfx::materials{}, "glb2", path);
			FAIL() << "the export did not fail";
		} catch (std::runtime_error const& error) {
			EXPECT_STREQ(error.what(),
			             "cannot export `bad`:\n"
			             "- active object `door` names group `nowhere` in state `opened`, which is not a node of "
			             "the model\n"
			             "- active object `lamp` describes no light of the model");
		}
	}

	// ---- transitions -----------------------------------------------------

	TEST(interactions_rules, transition_none) {
		expect_items("none", {});
		expect_items("  NONE ", {});
	}

	TEST(interactions_rules, transition_parts) {
		expect_items("150ms", {{.duration = 0.15}});
		expect_items("rotate.x 200ms ease-in, rotate.y 200ms ease-out 1s",
		             {{.channel = "rotate.x", .easing = easing::ease_in, .duration = 0.2},
		              {.channel = "rotate.y", .easing = easing::ease_out, .duration = 0.2, .delay = 1}});
		// any order, any case
		expect_items("Linear 2S ALL", {{.easing = easing::linear, .duration = 2}});
		expect_items("0ms linear 45ms", {{.easing = easing::linear, .delay = 0.045}});
		expect_items("ease-in-out\tset\n1s", {{.channel = "set", .easing = easing::ease_in_out, .duration = 1}});
	}

	TEST(interactions_rules, transition_times) {
		expect_items(".5s +2s", {{.duration = 0.5, .delay = 2}});
		expect_items("1e3ms", {{.duration = 1}});
		expect_items("1.5E+1ms", {{.duration = 0.015}});
		// time does not run backward
		expect_items("-1s 0.5s", {{.delay = 0.5}});
	}

	TEST(interactions_rules, transition_errors) {
		EXPECT_EQ(error_of("0"), "has `0`, which is not a channel, a timing function or a time");
		EXPECT_EQ(error_of("1.s"), "has `1.s`, which is not a channel, a timing function or a time");
		EXPECT_EQ(error_of("1sec"), "has `1sec`, which is not a channel, a timing function or a time");
		EXPECT_EQ(error_of("all set"), "names two channels, `all` and `set`");
		EXPECT_EQ(error_of("ease linear"), "has two timing functions, including `linear`");
		EXPECT_EQ(error_of("1s 2s 3s"), "has more than two times, including `3s`");
		EXPECT_EQ(error_of("1s,,2s"), "has an empty item");
		EXPECT_EQ(error_of("1s,"), "has an empty item");
		EXPECT_EQ(error_of(""), "has an empty item");
		EXPECT_EQ(error_of("1s, none"), "has `none`, which is not a channel, a timing function or a time");
		EXPECT_EQ(error_of("cubic-bezier(0, 0, 1, 1)"),
		          "has `cubic-bezier(0`, which is not a channel, a timing function or a time");
	}
}  // namespace pyxel::io::testing
