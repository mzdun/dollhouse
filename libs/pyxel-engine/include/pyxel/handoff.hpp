// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <utility>

namespace pyxel {
	// A slot passing the newest result of a worker thread to the render
	// thread. The worker post()s and goes on; the render thread take()s at
	// the start of a frame and uses the value on its own, with no lock held
	// during the frame. Neither side waits longer than a move of the value.
	// A value posted over one not taken yet replaces it: only the newest
	// result counts (see Handing the model over in docs/threads.md).
	template <typename T>
	class handoff {
	public:
		// any thread; the replaced value, if any, is destroyed on this
		// thread, after the lock is released
		void post(T value) {
			std::optional<T> dropped{};
			{
				std::lock_guard lock{mutex_};
				dropped = std::exchange(pending_, std::move(value));
				ready_.store(true, std::memory_order_relaxed);
			}
		}

		// the consumer; empty, if nothing new was posted since the last
		// take; without a new value, costs one atomic load and no lock
		std::optional<T> take() {
			// the flag only spares the lock; the lock orders the value
			if (!ready_.load(std::memory_order_relaxed)) return std::nullopt;
			std::lock_guard lock{mutex_};
			ready_.store(false, std::memory_order_relaxed);
			return std::exchange(pending_, std::nullopt);
		}

	private:
		std::mutex mutex_{};
		std::optional<T> pending_{};
		std::atomic<bool> ready_{false};
	};
}  // namespace pyxel
