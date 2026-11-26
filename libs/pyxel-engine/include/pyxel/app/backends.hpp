// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <chrono>
#include <cstdint>
#include <pyxel/app/event.hpp>
#include <pyxel/app/window.hpp>
#include <string>
#include <vector>

namespace pyxel::app {
	using seconds = std::chrono::duration<float>;

	// one fact about a backend, shown as "[source] label: value"
	struct info_line {
		std::string source{};
		std::string label{};
		std::string value{};

		bool operator==(info_line const&) const noexcept = default;
	};

	// a part of the stack telling what it runs on (driver, present mode);
	// the facts change rarely, if ever, so they get cached: revision() moves
	// on each time describe() would give something new
	struct info_source {
		virtual ~info_source();

		virtual void describe(std::vector<info_line>&) const {}
		virtual std::uint64_t revision() const noexcept { return 0; }
	};

	// the OS side: the event queue of the whole process
	struct platform : info_source {
		~platform() override;

		// appends the pending events, translated into engine events
		virtual void poll(std::vector<event>& events) = 0;
		// called instead of a frame, when there is nothing to draw
		virtual void idle() = 0;
	};

	// the GPU side: draws one frame into the window it was attached to
	struct renderer : info_source {
		~renderer() override;

		virtual void render() = 0;
		virtual void wait_idle() = 0;
	};

	// the UI layer as seen by the loop; pyxel-ui implements it with Dear
	// ImGui, the application's draw_ui() runs between the two calls
	struct ui : info_source {
		~ui() override;

		virtual void new_frame(window const& target, seconds dt) = 0;
		virtual void end_frame() = 0;
	};

	// the backends picked by the application, alive for the whole loop;
	// the implementation owns them and destroys them in the right order
	struct stack {
		virtual ~stack();

		virtual app::platform& platform() noexcept = 0;
		virtual app::window& window() noexcept = 0;
		virtual app::renderer& renderer() noexcept = 0;
		virtual app::ui* ui() noexcept { return nullptr; }

		// the lines of the platform, the renderer and the UI, in that order
		void collect_info(std::vector<info_line>& lines);
		// moves on, when collect_info() would give something new
		std::uint64_t info_revision();
	};
}  // namespace pyxel::app
