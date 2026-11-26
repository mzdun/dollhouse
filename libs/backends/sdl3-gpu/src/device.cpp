// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/sdl3/gpu/device.hpp>

namespace pyxel::sdl3::gpu {
	device_ptr create_device(bool try_vulkan) {
		SDL_GPUShaderFormat const formats =
		    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB;
#ifdef NDEBUG
		constexpr bool debug_mode = false;
#else
		constexpr bool debug_mode = true;
#endif
		device_ptr device{};
		if (try_vulkan) device.reset(SDL_CreateGPUDevice(formats, debug_mode, "vulkan"));
		if (!device) device.reset(SDL_CreateGPUDevice(formats, debug_mode, nullptr));
		if (!device) throw_error("SDL_CreateGPUDevice");

		return device;
	}

	window_claim::window_claim(SDL_GPUDevice* device, SDL_Window* window) : device_{device}, window_{window} {
		if (!SDL_ClaimWindowForGPUDevice(device_, window_)) throw_error("SDL_ClaimWindowForGPUDevice");
	}

	window_claim::~window_claim() { SDL_ReleaseWindowFromGPUDevice(device_, window_); }
}  // namespace pyxel::sdl3::gpu
