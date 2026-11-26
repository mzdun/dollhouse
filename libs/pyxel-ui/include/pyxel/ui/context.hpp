// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <imgui.h>
#include <pyxel/app/backends.hpp>
#include <pyxel/handles.hpp>

namespace pyxel::ui {
	using imgui_context_ptr = c_ptr<ImGuiContext, ImGui::DestroyContext>;
	static_assert(sizeof(imgui_context_ptr) == sizeof(ImGuiContext*));

	struct context_options {
		// the content scale of the display, see app::window::content_scale()
		float scale{1.0f};
		bool docking{true};
	};

	// the Dear ImGui context of the application, with its style, font and
	// docking; hands over the draw data (ImGui::GetDrawData()) at the end of
	// each frame and does not draw it, so it runs with no window and no GPU
	class context : public app::ui {
	public:
		explicit context(context_options const& options = {});
		~context() override;

		context(context const&) = delete;
		context& operator=(context const&) = delete;

		void new_frame(app::window const& target, app::seconds dt) override;
		void end_frame() override;

		ImGuiContext* get() const noexcept { return ctx_.get(); }

	private:
		imgui_context_ptr ctx_;
	};
}  // namespace pyxel::ui
