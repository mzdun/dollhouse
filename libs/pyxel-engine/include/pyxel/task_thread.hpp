// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>

namespace pyxel {
	// A thread running one task, again and again, each time it is woken up.
	// The thread starts with the object and sleeps until the first wake().
	// Wake-ups do not queue up: any number of them coming in while the task
	// runs cost one more run, not one each (see Requests while loading in
	// docs/threads.md).
	//
	// The task is fixed for the life of the thread. The data it reads may
	// be written before a wake(): the wake-up orders the writes before the
	// run it starts. The task gets the stop token of the thread and should
	// look at it between its steps; it must not throw.
	class task_thread {
	public:
		using task_type = std::function<void(std::stop_token)>;

		explicit task_thread(task_type task);
		task_thread(task_thread const&) = delete;
		task_thread& operator=(task_thread const&) = delete;
		// shutdown()
		~task_thread();

		// asks for one more run of the task; safe to call from any thread;
		// does nothing after shutdown()
		void wake();
		// asks the task to stop, waits for a running one to end and stops
		// the thread; later calls do nothing; not to be called from the
		// task itself
		void shutdown();

	private:
		void loop(std::stop_token token);

		task_type task_;
		std::mutex mutex_{};
		std::condition_variable_any wakeup_{};
		bool pending_{false};
		// last: starts after the members above, stops before them
		std::jthread thread_;
	};
}  // namespace pyxel
