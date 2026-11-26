// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/sdl3/handles.hpp>

namespace pyxel::sdl3::gpu {
	using device_ptr = c_ptr<SDL_GPUDevice, SDL_DestroyGPUDevice>;
	static_assert(sizeof(device_ptr) == sizeof(SDL_GPUDevice*));

	// Vulkan first, the platform's native driver as the fallback; the chosen
	// driver goes to the log; throws, if there is none
	device_ptr create_device(bool try_vulkan);

	// a window claimed by a device is released through that device, so the
	// claim keeps both; the device and the window must outlive it
	class window_claim {
	public:
		window_claim(SDL_GPUDevice* device, SDL_Window* window);  // throws
		~window_claim();

		window_claim(window_claim const&) = delete;
		window_claim& operator=(window_claim const&) = delete;

		SDL_GPUDevice* device() const noexcept { return device_; }
		SDL_Window* window() const noexcept { return window_; }

	private:
		SDL_GPUDevice* device_;
		SDL_Window* window_;
	};
}  // namespace pyxel::sdl3::gpu
