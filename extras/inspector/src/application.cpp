// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "application.hpp"

#include <imgui.h>
#include <format>
#include <memory>
#include <pyxel/base/str.hpp>
#include <pyxel/sdl3/imgui_stack.hpp>
#include <pyxel/version.hpp>
#include <string>
#include <utility>
#include <variant>

namespace inspector {
	using namespace std::literals;

	std::string_view application::name() const noexcept { return "inspector"sv; }

	std::string application::title() const { return "Inspector"s; }

	bool application::init(::args::args_view const& args) {
		options_ = parse_options(args);

		// TODO: check other runs for the model path

		if (options_.model_path) {
			loader_.open(*options_.model_path);
			loader_.reload();
		}
		return true;
	}

	std::unique_ptr<pyxel::app::stack> application::create_stack(pyxel::app::window_options const& options,
	                                                             pyxel::app::driver_hints const& hints) {
		return std::make_unique<pyxel::sdl3::imgui_gpu_stack>(options, hints);
	}

	void application::setup() {
		log_.header = std::format("{} {}", title(), PYXEL_VERSION_STR);
		frame_camera();
	}

	void application::update(pyxel::app::seconds) {
		if (auto result = loader_.take()) apply(std::move(*result));
		if (pixel_size() != frame_size_) frame_camera();
	}

	void application::draw_ui() {
		ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
		draw_menu();
		log_.draw(*this);
		if (show_demo_) ImGui::ShowDemoWindow(&show_demo_);
	}

	void application::shutdown() {
		// TODO: stop the watcher first, it wakes the loader up
		loader_.shutdown();
	}

	void application::draw_menu() {
		if (!ImGui::BeginMainMenuBar()) return;

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Reload", nullptr, false, options_.model_path.has_value())) loader_.reload();
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) quit();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("ImGui demo", nullptr, &show_demo_);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	void application::apply(load_result result) {
		if (auto* next = std::get_if<loaded>(&result)) {
			model_ = std::move(next->model);
			notes_ = std::move(next->notes);
			error_.clear();
			update_title();
			if (next->resets != resets_applied_) {
				resets_applied_ = next->resets;
				// TODO: reset the states of the active objects, once there are some
				frame_camera();
			}
			return;
		}

		// a failed load keeps the previous model
		error_ = std::move(std::get<failed>(result).message);
	}

	void application::frame_camera() {
		frame_size_ = pixel_size();
		auto const aspect = frame_size_.height > 0
		                        ? static_cast<float>(frame_size_.width) / static_cast<float>(frame_size_.height)
		                        : 1.0f;
		camera_ = pyxel::camera::framing(model_.bounds(), aspect);
	}

	void application::update_title() {
		auto document_title = model_.title;

		if (options_.model_path) {
			auto const filename = pyxel::as_str(options_.model_path->filename().u8string());
			if (document_title.empty())
				document_title = std::move(filename);
			else
				document_title = std::format("{} [{}]", document_title, filename);
		}

		set_title(document_title.empty() ? title() : std::format("{} - {}", document_title, title()));
	}
}  // namespace inspector
