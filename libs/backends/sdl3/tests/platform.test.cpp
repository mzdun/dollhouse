// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/sdl3/platform.hpp>
#include <variant>
#include <vector>

namespace pyxel::sdl3::testing {
	namespace {
		// no display needed
		struct dummy_video : ::testing::Test {
			void SetUp() override { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"); }
		};

		struct counting_listener : event_listener {
			int seen{};
			void on_sdl_event(SDL_Event const&) override { ++seen; }
		};

		void push(SDL_EventType type, SDL_WindowID window = 0) {
			SDL_Event event{};
			event.type = type;
			event.window.windowID = window;
			ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
		}
	}  // namespace

	using platform_test = dummy_video;

	TEST_F(platform_test, translates_events) {
		platform sdl{};
		std::vector<app::event> drained{};
		sdl.poll(drained);

		counting_listener listener{};
		sdl.set_listener(&listener);

		push(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 42);
		push(SDL_EVENT_DISPLAY_ADDED);
		push(SDL_EVENT_QUIT);

		std::vector<app::event> events{};
		sdl.poll(events);
		ASSERT_EQ(2u, events.size());
		ASSERT_TRUE(std::holds_alternative<app::window_close_requested>(events[0]));
		EXPECT_EQ(42u, std::get<app::window_close_requested>(events[0]).window);
		EXPECT_TRUE(std::holds_alternative<app::quit_requested>(events[1]));
		EXPECT_GE(listener.seen, 3);

		sdl.set_listener(nullptr);
		sdl.idle();
	}

	TEST_F(platform_test, opens_window) {
		platform sdl{};
		auto window = sdl.open_window({.title = "test", .size = {320, 200}});
		ASSERT_NE(nullptr, window);
		ASSERT_NE(nullptr, window->native());

		EXPECT_NE(0u, window->id());
		auto const size = window->size();
		EXPECT_GT(size.width, 0);
		EXPECT_GT(size.height, 0);
		auto const pixels = window->pixel_size();
		EXPECT_GE(pixels.width, size.width);
		EXPECT_GE(pixels.height, size.height);
		EXPECT_GT(window->content_scale(), 0.0f);
		EXPECT_FALSE(window->minimized());
		window->show();
	}
}  // namespace pyxel::sdl3::testing
