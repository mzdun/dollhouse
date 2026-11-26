// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <imgui_impl_sdl3.h>
#include <memory>
#include <pyxel/app/driver.hpp>
#include <pyxel/sdl3/gpu/device.hpp>
#include <pyxel/sdl3/gpu/imgui_pass.hpp>
#include <pyxel/sdl3/gpu/renderer.hpp>
#include <pyxel/sdl3/platform.hpp>
#include <pyxel/ui/context.hpp>

namespace pyxel::sdl3 {
	// feeds ImGui from SDL's own events through imgui_impl_sdl3
	//
	// TEMPORARY: pyxel-ui is to be fed from engine events instead (see
	// docs/ui-framework-analysis.md); this goes away once the engine events
	// carry the keyboard, mouse, text and gamepads
	class imgui_input : event_listener {
	public:
		imgui_input(platform& events, window const& target);  // throws
		~imgui_input() override;

		imgui_input(imgui_input const&) = delete;
		imgui_input& operator=(imgui_input const&) = delete;

		void new_frame();

	private:
		void on_sdl_event(SDL_Event const& event) override;

		platform& events_;
		init_guard<ImGui_ImplSDL3_Shutdown> backend_;
	};

	// SDL3 window and SDL GPU renderer with a Dear ImGui UI over a cleared
	// swapchain; the stack of Inspector and Dollhouse
	class imgui_gpu_stack final : public app::stack, app::ui {
	public:
		imgui_gpu_stack(app::window_options const& options, app::driver_hints const& hints);
		~imgui_gpu_stack() override;

		app::platform& platform() noexcept override { return platform_; }
		app::window& window() noexcept override { return *window_; }
		app::renderer& renderer() noexcept override { return renderer_; }
		app::ui* ui() noexcept override { return this; }

		gpu::renderer& gpu() noexcept { return renderer_; }

	private:
		void new_frame(app::window const& target, app::seconds dt) override;
		void end_frame() override;

		// destroyed bottom-up: the ImGui backends, the ImGui context, the
		// window claim, the device, the window and SDL itself last
		sdl3::platform platform_;
		std::unique_ptr<sdl3::window> window_;
		gpu::device_ptr device_;
		gpu::renderer renderer_;
		pyxel::ui::context ui_;
		imgui_input input_;
		gpu::imgui_pass pass_;
	};
}  // namespace pyxel::sdl3
