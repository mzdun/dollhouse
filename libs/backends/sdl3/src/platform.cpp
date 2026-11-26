// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <format>
#include <memory>
#include <pyxel/sdl3/platform.hpp>
#include <stdexcept>

namespace pyxel::sdl3 {
	void throw_error(std::string_view what) { throw std::runtime_error{std::format("{}: {}", what, SDL_GetError())}; }

	event_listener::~event_listener() = default;

	platform::platform() : session_{SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)} {
		if (!session_) throw_error("SDL_Init");
	}

	platform::~platform() = default;

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
}  // namespace pyxel::sdl3
