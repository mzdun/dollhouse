// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/sdl3/window.hpp>
#include <string>

namespace pyxel::sdl3 {
	namespace {
		float usable_scale(float scale) noexcept { return scale > 0.0f ? scale : 1.0f; }

		int scaled(int size, float scale) noexcept {
			return static_cast<int>(static_cast<float>(size) * usable_scale(scale));
		}

		window_ptr create(app::window_options const& options) {
			auto const scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
			window_ptr result{SDL_CreateWindow(
			    options.title.c_str(), scaled(options.size.width, scale), scaled(options.size.height, scale),
			    SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY)};
			if (!result) throw_error("SDL_CreateWindow");
			return result;
		}
	}  // namespace

	window::window(app::window_options const& options) : ptr_{create(options)} {}

	window::~window() = default;

	app::window_id window::id() const noexcept { return SDL_GetWindowID(ptr_.get()); }

	app::extent window::size() const noexcept {
		app::extent result{};
		SDL_GetWindowSize(ptr_.get(), &result.width, &result.height);
		return result;
	}

	app::extent window::pixel_size() const noexcept {
		app::extent result{};
		SDL_GetWindowSizeInPixels(ptr_.get(), &result.width, &result.height);
		return result;
	}

	float window::content_scale() const noexcept {
		return usable_scale(SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(ptr_.get())));
	}

	bool window::minimized() const noexcept { return (SDL_GetWindowFlags(ptr_.get()) & SDL_WINDOW_MINIMIZED) != 0; }

	void window::show() {
		if (!SDL_ShowWindow(ptr_.get())) throw_error("SDL_ShowWindow");
	}

	void window::set_title(std::string const& title) {
		if (!SDL_SetWindowTitle(ptr_.get(), title.c_str())) throw_error("SDL_SetWindowTitle");
	}
}  // namespace pyxel::sdl3
