// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <imgui_impl_sdlgpu3.h>
#include <pyxel/sdl3/gpu/renderer.hpp>

namespace pyxel::sdl3::gpu {
	// draws ImGui::GetDrawData() of the current ImGui context with
	// imgui_impl_sdlgpu3; create it after the ImGui context and destroy it
	// before, with the GPU idle
	class imgui_pass : public pass {
	public:
		explicit imgui_pass(renderer const& target);  // throws
		~imgui_pass() override;

		imgui_pass(imgui_pass const&) = delete;
		imgui_pass& operator=(imgui_pass const&) = delete;

		void prepare(SDL_GPUCommandBuffer* commands) override;
		void record(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* target) override;

	private:
		init_guard<ImGui_ImplSDLGPU3_Shutdown> backend_;
		ImDrawData* draw_data_{};
	};
}  // namespace pyxel::sdl3::gpu
