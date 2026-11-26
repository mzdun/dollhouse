// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/ui/context.hpp>

namespace pyxel::ui {
	namespace {
		constexpr float fallback_dt = 1.0f / 60.0f;
	}  // namespace

	context::context(context_options const& options) {
		IMGUI_CHECKVERSION();
		ctx_.reset(ImGui::CreateContext());

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
		if (options.docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.Fonts->AddFontDefaultVector();

		ImGui::StyleColorsDark();  // TODO: follow the OS theme

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(options.scale);
		style.FontScaleDpi = options.scale;
	}

	context::~context() = default;

	void context::new_frame(app::window const& target, app::seconds dt) {
		ImGui::SetCurrentContext(ctx_.get());

		auto const size = target.size();
		auto const pixels = target.pixel_size();

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2{static_cast<float>(size.width), static_cast<float>(size.height)};
		if (size.width > 0 && size.height > 0) {
			io.DisplayFramebufferScale = ImVec2{static_cast<float>(pixels.width) / static_cast<float>(size.width),
			                                    static_cast<float>(pixels.height) / static_cast<float>(size.height)};
		}
		io.DeltaTime = dt.count() > 0.0f ? dt.count() : fallback_dt;

		ImGui::NewFrame();
	}

	void context::end_frame() { ImGui::Render(); }
}  // namespace pyxel::ui
