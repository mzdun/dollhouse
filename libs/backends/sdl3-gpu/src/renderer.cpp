// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <initializer_list>
#include <pyxel/sdl3/gpu/renderer.hpp>
#include <string>
#include <string_view>
#include <vector>

using namespace std::literals;

namespace pyxel::sdl3::gpu {
	namespace {
		SDL_GPUPresentMode best_present_mode(SDL_GPUDevice* device, SDL_Window* window) {
			for (auto const mode : {SDL_GPU_PRESENTMODE_MAILBOX, SDL_GPU_PRESENTMODE_IMMEDIATE}) {
				if (SDL_WindowSupportsGPUPresentMode(device, window, mode)) return mode;
			}
			return SDL_GPU_PRESENTMODE_VSYNC;
		}

		std::string driver_name(char const* driver) {
			auto const name = driver ? driver : "unknown"sv;
			if (name == "vulkan"sv) return "Vulkan"s;
			if (name == "direct3d12"sv) return "Direct3D 12"s;
			if (name == "metal"sv) return "Metal"s;
			return std::string{name};
		}

		std::string vsync_name(SDL_GPUPresentMode mode) {
			switch (mode) {
				case SDL_GPU_PRESENTMODE_VSYNC:
					return "on"s;
				case SDL_GPU_PRESENTMODE_IMMEDIATE:
					return "off"s;
				case SDL_GPU_PRESENTMODE_MAILBOX:
					return "mailbox"s;
			}
			return "unknown"s;
		}
	}  // namespace

	pass::~pass() = default;

	renderer::renderer(SDL_GPUDevice* device, sdl3::window const& window, bool vsync)
	    : claim_{device, window.native()} {
		present_mode_ = vsync ? SDL_GPU_PRESENTMODE_VSYNC : best_present_mode(device, window.native());
		if (!SDL_SetGPUSwapchainParameters(device, window.native(), composition(), present_mode_))
			throw_error("SDL_SetGPUSwapchainParameters");
	}

	renderer::~renderer() = default;

	SDL_GPUTextureFormat renderer::swapchain_format() const noexcept {
		return SDL_GetGPUSwapchainTextureFormat(claim_.device(), claim_.window());
	}

	void renderer::render() {
		SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(claim_.device());
		if (!commands) throw_error("SDL_AcquireGPUCommandBuffer");

		SDL_GPUTexture* swapchain = nullptr;
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, claim_.window(), &swapchain, nullptr, nullptr)) {
			SDL_CancelGPUCommandBuffer(commands);
			throw_error("SDL_WaitAndAcquireGPUSwapchainTexture");
		}

		if (!swapchain) {
			// no image this frame (e.g. the window is being minimized)
			SDL_SubmitGPUCommandBuffer(commands);
			return;
		}

		for (auto* next : passes_)
			next->prepare(commands);

		SDL_GPUColorTargetInfo target{};
		target.texture = swapchain;
		target.clear_color = clear_color;
		target.load_op = SDL_GPU_LOADOP_CLEAR;
		target.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
		for (auto* next : passes_)
			next->record(commands, render_pass);
		SDL_EndGPURenderPass(render_pass);

		if (!SDL_SubmitGPUCommandBuffer(commands)) throw_error("SDL_SubmitGPUCommandBuffer");
	}

	void renderer::wait_idle() { SDL_WaitForGPUIdle(claim_.device()); }

	void renderer::describe(std::vector<app::info_line>& lines) const {
		lines.push_back({.source = "SDL", .label = "Driver", .value = driver_name(SDL_GetGPUDeviceDriver(device()))});
		lines.push_back({.source = "SDL", .label = "VSync", .value = vsync_name(present_mode_)});
	}
}  // namespace pyxel::sdl3::gpu
