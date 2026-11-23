// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx/transition.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::gfx::interactions::testing {
	namespace {
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
	}  // namespace

	// ---- transitions -----------------------------------------------------

	TEST(gfx_transition, transition_none) {
		expect_items("none", {});
		expect_items("  NONE ", {});
	}

	TEST(gfx_transition, transition_parts) {
		expect_items("150ms", {{.duration = 0.15}});
		expect_items("rotate.x 200ms ease-in, rotate.y 200ms ease-out 1s",
		             {{.channel = "rotate.x", .easing = easing::ease_in, .duration = 0.2},
		              {.channel = "rotate.y", .easing = easing::ease_out, .duration = 0.2, .delay = 1}});
		// any order, any case
		expect_items("Linear 2S ALL", {{.easing = easing::linear, .duration = 2}});
		expect_items("0ms linear 45ms", {{.easing = easing::linear, .delay = 0.045}});
		expect_items("ease-in-out\tset\n1s", {{.channel = "set", .easing = easing::ease_in_out, .duration = 1}});
	}

	TEST(gfx_transition, transition_times) {
		expect_items(".5s +2s", {{.duration = 0.5, .delay = 2}});
		expect_items("1e3ms", {{.duration = 1}});
		expect_items("1.5E+1ms", {{.duration = 0.015}});
		// time does not run backward
		expect_items("-1s 0.5s", {{.delay = 0.5}});
	}

	TEST(gfx_transition, transition_errors) {
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

	TEST(gfx_transition, timing_of_an_item) {
		single_transition const item{.channel = "set", .easing = easing::linear, .duration = 0.2, .delay = 1};
		EXPECT_EQ(item.timing(), (timing{.easing = easing::linear, .duration = 0.2, .delay = 1}));
		EXPECT_EQ(single_transition{}.timing(), timing{});
	}
}  // namespace pyxel::gfx::interactions::testing
