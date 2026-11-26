// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/app/event.hpp>
#include <string>

namespace pyxel::app {
	struct extent {
		int width{};
		int height{};

		bool operator==(extent const&) const noexcept = default;
	};

	struct window_options {
		std::string title{};
		// in screen coordinates at the scale of 1.0; the backend scales it
		// for the display the window opens on
		extent size{1280, 800};
	};

	// the window is only a window: no camera, HUD or font lives here
	struct window {
		virtual ~window();

		virtual window_id id() const noexcept = 0;
		// in screen coordinates, the unit of mouse positions
		virtual extent size() const noexcept = 0;
		// in pixels of the drawable surface
		virtual extent pixel_size() const noexcept = 0;
		// the scale the UI should use on the window's display
		virtual float content_scale() const noexcept = 0;
		virtual bool minimized() const noexcept = 0;
		virtual void show() = 0;
		// UTF-8
		virtual void set_title(std::string const& title) = 0;
	};
}  // namespace pyxel::app
