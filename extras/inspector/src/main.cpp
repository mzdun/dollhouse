// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <imgui.h>
#include <array>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <print>
#include <pyxel/app/application.hpp>
#include <pyxel/app/main_impl.hpp>
#include <pyxel/base/str.hpp>
#include <pyxel/camera.hpp>
#include <pyxel/io/import.hpp>
#include <pyxel/sdl3/imgui_stack.hpp>
#include <pyxel/version.hpp>
#include <string>
#include <string_view>
#include <utility>

ENUM_TRAITS_BEGIN(pyxel::app::gpu_api)
ENUM_TRAITS_NAME(vulkan)
ENUM_TRAITS_NAME(native)
ENUM_TRAITS_END(pyxel::app::gpu_api)

template <>
struct args::converter<std::filesystem::path> {
	static std::filesystem::path value(args::parser&, std::string const& arg, std::string const&) {
		// std::string c-tor uses OS code page, u8string_view c-tor uses UTF-8 characters
		return std::filesystem::path{pyxel::as_u8v(arg)};
	}
};

namespace {
	[[noreturn]] void print_version() {
		std::print("inspector {}\n", pyxel::version::ui);
		std::exit(1);
	}

	[[noreturn]] void file_error(std::filesystem::path const& path, std::string_view message) {
		std::print(std::cerr, "{}: error: {}\n", pyxel::as_sv(path.u8string()), message);
		std::exit(1);
	}
}  // namespace

namespace inspector {
	using namespace std::literals;

	template <typename T, size_t N>
	struct moving_average {
		static_assert(N > 2);
		std::array<T, N> values{};
		T running_sum{};
		size_t used{0};
		size_t index{0};

		T observe(T value) {
			if (index == N) {
				index = 0;
			}
			running_sum -= values[index];
			values[index] = value;
			++index;
			running_sum += value;

			if (used < N) {
				++used;
			}

			return running_sum / static_cast<T>(used);
		}
	};

	struct application : pyxel::app::application {
		std::string_view name() const noexcept override { return "inspector"sv; }
		std::string title() const override { return "Inspector"s; }

		bool init(::args::args_view const& args) override {
			args::null_translator tr{};
			args::parser parser{"One piece of furniture, every angle", args, &tr};
			parser.custom(print_version, "version").help("show the version and exit").opt();
			parser.set<std::true_type>(verbose_, "v").help("show more information").opt();
			parser.set<std::true_type>(driver_.vsync, "vsync").help("turns the VSync ON").opt();
			parser.set<std::false_type>(driver_.vsync, "no-vsync").help("turns the VSync OFF").opt();
			parser.arg(driver_.preferred_api, "driver")
			    .meta("vulkan|native")
			    .help("selects GPU driver preference")
			    .opt();
			parser.arg(model_path_).meta("<model>").help("inspect model from the path");

			parser.parse();

			if (model_path_) {
				if (!std::filesystem::exists(*model_path_)) {
					file_error(*model_path_, "model does not exists"sv);
				}

				if (!std::filesystem::is_regular_file(*model_path_)) {
					file_error(*model_path_, "model needs to be a file"sv);
				}

				model_path_ = std::filesystem::weakly_canonical(*model_path_);
				std::print("{}\n", pyxel::as_sv(model_path_->u8string()));
			}

			// TODO: check other runs for model_path_

			return true;
		}

		pyxel::app::driver_hints driver() const override { return driver_; }

		std::unique_ptr<pyxel::app::stack> create_stack(pyxel::app::window_options const& options,
		                                                pyxel::app::driver_hints const& hints) override {
			return std::make_unique<pyxel::sdl3::imgui_gpu_stack>(options, hints);
		}

		void setup() override {
			header_ = std::format("{} {}", title(), PYXEL_VERSION_STR);
			if (model_path_) {
				imported_ = pyxel::io::import_model(*model_path_);
				update_title();
			}
			frame_camera();
		}

		void update(pyxel::app::seconds) override {
			if (pixel_size() != frame_size_) frame_camera();
		}

		void refresh() {
			// TODO: remove old import notes from log
			if (!model_path_) {
				return;
			}

			imported_ = pyxel::io::import_model(*model_path_);

			// TODO: add new import notes to log
			update_title();
			frame_camera();
		}

		void draw_ui() override {
			ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Exit")) quit();
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("View")) {
					ImGui::MenuItem("ImGui demo", nullptr, &show_demo_);
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			if (ImGui::Begin("Log")) {
				ImGui::TextUnformatted(header_.c_str());
				ImGui::Text("%.1f FPS", avg_.observe(static_cast<double>(ImGui::GetIO().Framerate)));
				if (verbose_) {
					for (auto const& [source, label, value] : backend_info())
						ImGui::Text("[%s] %s: %s", source.c_str(), label.c_str(), value.c_str());
					ImGui::TextUnformatted(
					    std::format("[Camera] eye: {:.3f}, {:.3f}, {:.3f}", camera_.eye.x, camera_.eye.y, camera_.eye.z)
					        .c_str());
					ImGui::TextUnformatted(std::format("[Camera] target: {:.3f}, {:.3f}, {:.3f}", camera_.target.x,
					                                   camera_.target.y, camera_.target.z)
					                           .c_str());
					ImGui::TextUnformatted(
					    std::format("[Camera] near/far: {:.3f} / {:.3f}", camera_.near_plane, camera_.far_plane)
					        .c_str());
				}
			}
			ImGui::End();

			if (show_demo_) ImGui::ShowDemoWindow(&show_demo_);
		}

	private:
		// immovable for now: looks at the whole model, again for every new
		// model and every new size of the window
		void frame_camera() {
			frame_size_ = pixel_size();
			auto const aspect = frame_size_.height > 0
			                        ? static_cast<float>(frame_size_.width) / static_cast<float>(frame_size_.height)
			                        : 1.0f;
			camera_ = pyxel::camera::framing(imported_.model.bounds(), aspect);
		}

		void update_title() {
			auto document_title = imported_.model.title;

			if (model_path_) {
				auto const filename = pyxel::as_str(model_path_->filename().u8string());
				if (document_title.empty())
					document_title = std::move(filename);
				else
					document_title = std::format("{} [{}]", document_title, filename);
			}

			set_title(document_title.empty() ? title() : std::format("{} - {}", document_title, title()));
		}

		moving_average<double, 10000> avg_{};
		std::string header_{};
		pyxel::io::imported_model imported_{};
		pyxel::camera camera_{};
		pyxel::app::extent frame_size_{};
		std::optional<std::filesystem::path> model_path_{};
		pyxel::app::driver_hints driver_{};
		bool verbose_{false};
		bool show_demo_{false};
	};
}  // namespace inspector

RUN_APPLICATION(inspector::application);
