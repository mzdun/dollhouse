// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/handles.hpp>
#include <utility>

namespace pyxel::testing {
	namespace {
		int destroyed{};
		int shut_down{};

		struct thing {};
		void destroy_thing(thing* ptr) {
			++destroyed;
			delete ptr;
		}
		void shutdown_thing() { ++shut_down; }

		using thing_ptr = c_ptr<thing, destroy_thing>;
		using thing_session = init_guard<shutdown_thing>;
		static_assert(sizeof(thing_ptr) == sizeof(thing*));
	}  // namespace

	TEST(handles, c_ptr_destroys) {
		destroyed = 0;
		{
			thing_ptr ptr{new thing{}};
			thing_ptr empty{};
		}
		EXPECT_EQ(1, destroyed);
	}

	TEST(handles, init_guard_shuts_down_on_success_only) {
		shut_down = 0;
		{
			thing_session failed{false};
			EXPECT_FALSE(failed);
			thing_session ok{true};
			EXPECT_TRUE(ok);
		}
		EXPECT_EQ(1, shut_down);
	}

	TEST(handles, init_guard_moves) {
		shut_down = 0;
		{
			thing_session first{true};
			thing_session second{std::move(first)};
			EXPECT_FALSE(first);  // NOLINT(bugprone-use-after-move)
			EXPECT_TRUE(second);

			thing_session third{true};
			third = std::move(second);
			EXPECT_EQ(1, shut_down);
			EXPECT_TRUE(third);

			auto& self = third;
			third = std::move(self);
			EXPECT_TRUE(third);
		}
		EXPECT_EQ(2, shut_down);
	}
}  // namespace pyxel::testing
