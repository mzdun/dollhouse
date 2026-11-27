// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <format>
#include <memory>
#include <pyxel/sdl3/platform.hpp>
#include <stdexcept>
#include <utility>

#if defined(__SANITIZE_THREAD__)  // GCC
#define PYXEL_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)  // Clang
#define PYXEL_TSAN 1
#endif
#endif

#if defined(PYXEL_TSAN)
// SDL_Init and SDL_Quit go through libdbus, which takes two of its own
// mutexes in one order on the way up and in the other on the way down. The
// cycle is all inside the (uninstrumented) library and ThreadSanitizer
// reports it as a lock-order-inversion in every program starting SDL.
// The runtime reads this list next to the TSAN_OPTIONS suppressions file;
// it lives here, so every binary linking the SDL backend gets it.
extern "C" char const* __tsan_default_suppressions() { return "deadlock:libdbus-1.so\n"; }
#endif

namespace pyxel::sdl3 {
	void throw_error(std::string_view what) { throw std::runtime_error{std::format("{}: {}", what, SDL_GetError())}; }

	event_listener::~event_listener() = default;

	platform::platform() : session_{SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)} {
		if (!session_) throw_error("SDL_Init");
	}

	platform::~platform() { set_live_redraw({}); }

	std::unique_ptr<sdl3::window> platform::open_window(app::window_options const& options) const {
		return std::make_unique<sdl3::window>(options);
	}

	void platform::poll(std::vector<app::event>& events) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (listener_) listener_->on_sdl_event(event);

			switch (event.type) {
				case SDL_EVENT_QUIT:
					events.emplace_back(app::quit_requested{});
					break;
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					events.emplace_back(app::window_close_requested{.window = event.window.windowID});
					break;
				default:
					break;
			}
		}
	}

	void platform::idle() { SDL_Delay(10); }

	void platform::set_live_redraw(std::function<void()> redraw) {
		if (redraw_ && !redraw) SDL_RemoveEventWatch(watch_events, this);
		if (!redraw_ && redraw) {
			if (!SDL_AddEventWatch(watch_events, this)) throw_error("SDL_AddEventWatch");
		}
		redraw_ = std::move(redraw);
	}

	// may run on any thread, except for SDL_EVENT_WINDOW_EXPOSED, which
	// comes from the main thread
	bool SDLCALL platform::watch_events(void* self, SDL_Event* event) {
		if (event->type == SDL_EVENT_WINDOW_EXPOSED && event->window.data1 != 0) {
			auto const& redraw = static_cast<platform*>(self)->redraw_;
			if (redraw) redraw();
		}
		return true;
	}
}  // namespace pyxel::sdl3
