// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <memory>
#include <pyxel/app/backends.hpp>
#include <pyxel/sdl3/handles.hpp>
#include <pyxel/sdl3/window.hpp>
#include <vector>

namespace pyxel::sdl3 {
	// sees every SDL event before it is translated; for the glue that still
	// needs SDL's own events (imgui_impl_sdl3), until the engine events
	// carry everything
	struct event_listener {
		virtual ~event_listener();
		virtual void on_sdl_event(SDL_Event const& event) = 0;
	};

	// SDL itself: initialized in the constructor, quit in the destructor, so
	// it has to outlive every window
	class platform : public app::platform {
	public:
		platform();
		~platform() override;

		platform(platform const&) = delete;
		platform& operator=(platform const&) = delete;

		std::unique_ptr<sdl3::window> open_window(app::window_options const& options) const;

		void poll(std::vector<app::event>& events) override;
		void idle() override;

		// one listener at a time; nullptr removes it
		void set_listener(event_listener* listener) noexcept { listener_ = listener; }

	private:
		sdl_session session_;
		event_listener* listener_{};
	};
}  // namespace pyxel::sdl3
