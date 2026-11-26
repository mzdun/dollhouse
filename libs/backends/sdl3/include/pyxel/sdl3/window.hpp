// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/app/window.hpp>
#include <pyxel/sdl3/handles.hpp>
#include <string>

namespace pyxel::sdl3 {
	// an SDL window, opened hidden; show() it once it has something to draw
	class window : public app::window {
	public:
		// SDL has to be initialized; use platform::open_window()
		explicit window(app::window_options const& options);
		~window() override;

		app::window_id id() const noexcept override;
		app::extent size() const noexcept override;
		app::extent pixel_size() const noexcept override;
		float content_scale() const noexcept override;
		bool minimized() const noexcept override;
		void show() override;
		void set_title(std::string const& title) override;

		SDL_Window* native() const noexcept { return ptr_.get(); }

	private:
		window_ptr ptr_;
	};
}  // namespace pyxel::sdl3
