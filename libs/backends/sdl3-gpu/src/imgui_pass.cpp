// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/sdl3/gpu/imgui_pass.hpp>
#include <stdexcept>

namespace pyxel::sdl3::gpu {
	namespace {
		bool init(renderer const& target) {
			ImGui_ImplSDLGPU3_InitInfo info{};
			info.Device = target.device();
			info.ColorTargetFormat = target.swapchain_format();
			info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
			info.SwapchainComposition = target.composition();
			info.PresentMode = target.present_mode();
			return ImGui_ImplSDLGPU3_Init(&info);
		}

		bool drawable(ImDrawData const* draw_data) noexcept {
			return draw_data && draw_data->Valid && draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f;
		}
	}  // namespace

	imgui_pass::imgui_pass(renderer const& target) : backend_{init(target)} {
		if (!backend_) throw std::runtime_error{"ImGui_ImplSDLGPU3_Init failed"};
		// its only job is to create the device objects, once
		ImGui_ImplSDLGPU3_NewFrame();
	}

	imgui_pass::~imgui_pass() = default;

	void imgui_pass::prepare(SDL_GPUCommandBuffer* commands) {
		draw_data_ = ImGui::GetDrawData();
		if (!drawable(draw_data_)) {
			draw_data_ = nullptr;
			return;
		}
		ImGui_ImplSDLGPU3_PrepareDrawData(draw_data_, commands);
	}

	void imgui_pass::record(SDL_GPUCommandBuffer* commands, SDL_GPURenderPass* target) {
		if (draw_data_) ImGui_ImplSDLGPU3_RenderDrawData(draw_data_, commands, target);
		draw_data_ = nullptr;
	}
}  // namespace pyxel::sdl3::gpu
