// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/app/backends.hpp>
#include <pyxel/sdl3/gpu/device.hpp>
#include <pyxel/sdl3/window.hpp>
#include <vector>

namespace pyxel::sdl3::gpu {
	// one part of a frame; prepare() runs for every pass before the render
	// pass begins (uploads, copy passes), record() inside it
	struct pass {
		virtual ~pass();
		virtual void prepare(SDL_GPUCommandBuffer*) {}
		virtual void record(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* target) = 0;
	};

	// claims the window for the device and draws the passes into its
	// swapchain, cleared first
	class renderer : public app::renderer {
	public:
		renderer(SDL_GPUDevice* device, sdl3::window const& window, bool vsync);  // throws
		~renderer() override;

		void render() override;
		void wait_idle() override;
		// [SDL] Backend: <driver>, [SDL] VSync: on|off|mailbox
		void describe(std::vector<app::info_line>& lines) const override;

		// drawn in the order added; the renderer does not own the pass, and
		// no frame may be rendered after the pass is gone
		void add_pass(pass& next) { passes_.push_back(&next); }

		SDL_GPUDevice* device() const noexcept { return claim_.device(); }
		SDL_GPUTextureFormat swapchain_format() const noexcept;
		SDL_GPUSwapchainComposition composition() const noexcept { return SDL_GPU_SWAPCHAINCOMPOSITION_SDR; }
		SDL_GPUPresentMode present_mode() const noexcept { return present_mode_; }

		SDL_FColor clear_color{0.12f, 0.13f, 0.15f, 1.0f};

	private:
		window_claim claim_;
		SDL_GPUPresentMode present_mode_{SDL_GPU_PRESENTMODE_VSYNC};
		std::vector<pass*> passes_{};
	};
}  // namespace pyxel::sdl3::gpu
