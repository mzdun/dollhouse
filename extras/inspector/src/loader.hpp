// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/handoff.hpp>
#include <pyxel/io/import.hpp>
#include <pyxel/io/interactions.hpp>
#include <pyxel/task_thread.hpp>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace inspector {
	// what the loader hands over to the render thread (see Loader thread in
	// docs/threads.md)
	struct loaded {
		pyxel::gfx::loaded_model model;
		std::vector<pyxel::io::interactions::note> notes;
		// the resets asked for when the import started; a number other than
		// the one applied last starts the model over (see Starting over in
		// docs/threads.md)
		unsigned resets{};
	};

	struct failed {
		std::string message;
	};

	using load_result = std::variant<loaded, failed>;

	// The loader thread of the Inspector: imports the model on a thread of
	// its own and hands the result over to the render thread, which take()s
	// it at the start of a frame.
	class loader {
	public:
		// replace the list of files to watch, or add to it after a failed
		// import
		enum class observe_mode { reset, append };

		// the model to load from now on; does not load it
		void open(std::filesystem::path path);
		// loads the model again; the next model starts over, unlike a
		// reload from the watcher, which keeps what it can
		void reload();
		// the newest result not taken yet
		std::optional<load_result> take() { return results_.take(); }
		// an import is running
		bool loading() const noexcept { return loading_.load(std::memory_order_relaxed); }
		// waits for a running import to end and drops its result
		void shutdown() { thread_.shutdown(); }

	private:
		// the loader thread
		void load(std::stop_token token);
		// hands the files of the model over to the watcher, once there is
		// one
		void observe(std::vector<pyxel::io::source_file> sources, observe_mode mode);

		std::mutex path_mutex_{};
		std::optional<std::filesystem::path> path_{};
		pyxel::handoff<load_result> results_{};
		std::atomic<bool> loading_{false};
		// bumped by reload(), read at the start of an import
		std::atomic<unsigned> resets_requested_{0};
		// last: the thread starts after everything it reads is there, and
		// stops before any of it is gone
		pyxel::task_thread thread_{[this](std::stop_token token) { load(std::move(token)); }};
	};
}  // namespace inspector
