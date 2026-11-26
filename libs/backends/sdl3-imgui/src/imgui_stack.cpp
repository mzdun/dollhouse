// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/sdl3/imgui_stack.hpp>
#include <stdexcept>

namespace pyxel::sdl3 {
	imgui_input::imgui_input(platform& events, window const& target)
	    : events_{events}, backend_{ImGui_ImplSDL3_InitForSDLGPU(target.native())} {
		if (!backend_) throw std::runtime_error{"ImGui_ImplSDL3_InitForSDLGPU failed"};
		events_.set_listener(this);
	}

	imgui_input::~imgui_input() { events_.set_listener(nullptr); }

	void imgui_input::new_frame() { ImGui_ImplSDL3_NewFrame(); }

	void imgui_input::on_sdl_event(SDL_Event const& event) { ImGui_ImplSDL3_ProcessEvent(&event); }

	imgui_gpu_stack::imgui_gpu_stack(app::window_options const& options, app::driver_hints const& hints)
	    : window_{platform_.open_window(options)}
	    , device_{gpu::create_device(hints.preferred_api == app::gpu_api::vulkan)}
	    , renderer_{device_.get(), *window_, hints.vsync}
	    , ui_{{.scale = window_->content_scale()}}
	    , input_{platform_, *window_}
	    , pass_{renderer_} {
		renderer_.add_pass(pass_);
		window_->show();
	}

	// the passes release their GPU objects; nothing may still use them
	imgui_gpu_stack::~imgui_gpu_stack() { renderer_.wait_idle(); }

	void imgui_gpu_stack::new_frame(app::window const& target, app::seconds dt) {
		input_.new_frame();
		ui_.new_frame(target, dt);
	}

	void imgui_gpu_stack::end_frame() { ui_.end_frame(); }
}  // namespace pyxel::sdl3
