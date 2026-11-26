// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <imgui_impl_null.h>
#include <pyxel/ui/context.hpp>
#include <string>

namespace pyxel::ui::testing {
	namespace {
		using null_renderer = init_guard<ImGui_ImplNullRender_Shutdown>;

		struct fake_window : app::window {
			app::window_id id() const noexcept override { return 1; }
			app::extent size() const noexcept override { return {640, 400}; }
			app::extent pixel_size() const noexcept override { return {1280, 800}; }
			float content_scale() const noexcept override { return 2.0f; }
			bool minimized() const noexcept override { return false; }
			void show() override {}
			void set_title(std::string const&) override {}
		};
	}  // namespace

	TEST(context, configures_imgui) {
		context ui{{.scale = 2.0f}};
		ASSERT_EQ(ui.get(), ImGui::GetCurrentContext());

		auto const& io = ImGui::GetIO();
		EXPECT_TRUE(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
		EXPECT_TRUE(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
		EXPECT_EQ(2.0f, ImGui::GetStyle().FontScaleDpi);
	}

	TEST(context, no_docking) {
		context ui{{.docking = false}};
		EXPECT_FALSE(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable);
	}

	TEST(context, headless_frame) {
		context ui{};
		ImGui::GetIO().IniFilename = nullptr;
		null_renderer renderer{ImGui_ImplNullRender_Init()};
		ASSERT_TRUE(renderer);

		fake_window window{};
		for (auto dt : {0.0f, 0.25f}) {
			ui.new_frame(window, app::seconds{dt});

			auto const& io = ImGui::GetIO();
			EXPECT_EQ(640.0f, io.DisplaySize.x);
			EXPECT_EQ(400.0f, io.DisplaySize.y);
			EXPECT_EQ(2.0f, io.DisplayFramebufferScale.x);
			EXPECT_EQ(2.0f, io.DisplayFramebufferScale.y);
			EXPECT_GT(io.DeltaTime, 0.0f);

			ImGui::Begin("panel");
			ImGui::TextUnformatted("text");
			ImGui::End();
			ui.end_frame();

			auto const* draw_data = ImGui::GetDrawData();
			ASSERT_NE(nullptr, draw_data);
			EXPECT_TRUE(draw_data->Valid);
		}
		EXPECT_EQ(0.25f, ImGui::GetIO().DeltaTime);
		// a new window is hidden for its first frame, while it gets measured
		EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
	}
}  // namespace pyxel::ui::testing
