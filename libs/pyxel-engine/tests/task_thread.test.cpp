// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <atomic>
#include <pyxel/task_thread.hpp>
#include <semaphore>
#include <stop_token>

namespace pyxel::testing {
	TEST(task_thread, sleeps_until_woken) {
		std::atomic<int> runs{0};
		{
			task_thread thread{[&](std::stop_token) { ++runs; }};
		}
		EXPECT_EQ(runs, 0);
	}

	TEST(task_thread, runs_once_per_wake) {
		std::atomic<int> runs{0};
		std::counting_semaphore<> done{0};
		task_thread thread{[&](std::stop_token) {
			++runs;
			done.release();
		}};

		thread.wake();
		done.acquire();
		EXPECT_EQ(runs, 1);

		thread.wake();
		done.acquire();
		EXPECT_EQ(runs, 2);
	}

	TEST(task_thread, wakes_during_a_run_cost_one_more_run) {
		std::atomic<int> runs{0};
		std::counting_semaphore<> entered{0};
		std::counting_semaphore<> gate{0};
		task_thread thread{[&](std::stop_token) {
			++runs;
			entered.release();
			gate.acquire();
		}};

		thread.wake();
		entered.acquire();
		for (int i = 0; i < 10; ++i)
			thread.wake();
		gate.release();

		entered.acquire();
		gate.release();
		thread.shutdown();

		EXPECT_EQ(runs, 2);
	}

	TEST(task_thread, shutdown_stops_a_running_task) {
		std::counting_semaphore<> entered{0};
		std::atomic<bool> saw_stop{false};
		task_thread thread{[&](std::stop_token token) {
			std::binary_semaphore stopped{0};
			std::stop_callback on_stop{token, [&] { stopped.release(); }};
			entered.release();
			stopped.acquire();
			saw_stop = token.stop_requested();
		}};

		thread.wake();
		entered.acquire();
		thread.shutdown();

		EXPECT_TRUE(saw_stop);
	}

	TEST(task_thread, wake_after_shutdown_does_nothing) {
		std::atomic<int> runs{0};
		task_thread thread{[&](std::stop_token) { ++runs; }};

		thread.shutdown();
		thread.wake();
		thread.shutdown();

		EXPECT_EQ(runs, 0);
	}
}  // namespace pyxel::testing
