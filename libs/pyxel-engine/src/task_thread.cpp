// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/task_thread.hpp>
#include <utility>

namespace pyxel {
	task_thread::task_thread(task_type task)
	    : task_{std::move(task)}, thread_{[this](std::stop_token token) { loop(std::move(token)); }} {}

	task_thread::~task_thread() { shutdown(); }

	void task_thread::wake() {
		{
			std::lock_guard lock{mutex_};
			pending_ = true;
		}
		wakeup_.notify_one();
	}

	void task_thread::shutdown() {
		thread_.request_stop();
		if (thread_.joinable()) thread_.join();
	}

	void task_thread::loop(std::stop_token token) {
		while (true) {
			{
				std::unique_lock lock{mutex_};
				// returns when woken up or stopped; a stop wins over a
				// pending run
				wakeup_.wait(lock, token, [this] { return pending_; });
				if (token.stop_requested()) return;
				pending_ = false;
			}
			task_(token);
		}
	}
}  // namespace pyxel
