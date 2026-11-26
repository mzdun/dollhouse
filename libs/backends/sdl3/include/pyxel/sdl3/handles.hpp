// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <SDL3/SDL.h>
#include <pyxel/handles.hpp>
#include <string_view>

namespace pyxel::sdl3 {
	using window_ptr = c_ptr<SDL_Window, SDL_DestroyWindow>;
	using sdl_session = init_guard<SDL_Quit>;

	static_assert(sizeof(window_ptr) == sizeof(SDL_Window*));

	// throws std::runtime_error with "<what>: <SDL_GetError()>"
	[[noreturn]] void throw_error(std::string_view what);
}  // namespace pyxel::sdl3
