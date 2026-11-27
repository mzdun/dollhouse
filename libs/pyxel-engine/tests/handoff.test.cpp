// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <memory>
#include <pyxel/handoff.hpp>
#include <thread>

namespace pyxel::testing {
	TEST(handoff, starts_empty) {
		handoff<int> slot{};
		EXPECT_EQ(slot.take(), std::nullopt);
	}

	TEST(handoff, takes_once) {
		handoff<int> slot{};
		slot.post(5);
		EXPECT_EQ(slot.take(), 5);
		EXPECT_EQ(slot.take(), std::nullopt);
	}

	TEST(handoff, newest_wins) {
		handoff<std::unique_ptr<int>> slot{};
		slot.post(std::make_unique<int>(1));
		slot.post(std::make_unique<int>(2));

		auto value = slot.take();
		ASSERT_TRUE(value);
		ASSERT_TRUE(*value);
		EXPECT_EQ(**value, 2);
		EXPECT_EQ(slot.take(), std::nullopt);
	}

	TEST(handoff, crosses_threads) {
		static constexpr int last = 1000;
		handoff<int> slot{};

		std::jthread worker{[&] {
			for (int i = 1; i <= last; ++i)
				slot.post(i);
		}};

		int seen = 0;
		while (seen != last) {
			if (auto value = slot.take()) {
				EXPECT_GT(*value, seen);
				seen = *value;
			} else {
				std::this_thread::yield();
			}
		}
	}
}  // namespace pyxel::testing
