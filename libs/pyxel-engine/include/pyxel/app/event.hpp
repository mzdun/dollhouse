// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstdint>
#include <variant>

namespace pyxel::app {
	using window_id = std::uint32_t;

	// the whole application should close (last window closed, OS logout)
	struct quit_requested {};

	// the user asked to close one window
	struct window_close_requested {
		window_id window{};
	};

	// the engine's own events; backends translate theirs into these
	using event = std::variant<quit_requested, window_close_requested>;
}  // namespace pyxel::app
