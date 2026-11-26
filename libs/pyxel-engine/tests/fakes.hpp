// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <pyxel/app/application.hpp>
#include <pyxel/app/backends.hpp>
#include <string>
#include <utility>
#include <vector>

namespace pyxel::app::testing {
	using journal = std::vector<std::string>;

	struct fake_window : window {
		window_id id_{7};
		bool minimized_{false};
		std::string title_{};

		window_id id() const noexcept override { return id_; }
		extent size() const noexcept override { return {640, 400}; }
		extent pixel_size() const noexcept override { return {1280, 800}; }
		float content_scale() const noexcept override { return 1.5f; }
		bool minimized() const noexcept override { return minimized_; }
		void show() override {}
		void set_title(std::string const& title) override { title_ = title; }
	};

	// each poll() hands over the next batch of events; once the script runs
	// out, it asks to quit; before each batch, it redraws `live_redraws`
	// times, as a platform would while the user resizes the window
	struct fake_platform : platform {
		journal& log;
		std::deque<std::vector<event>> script{};
		int live_redraws{0};
		std::function<void()> redraw{};
		explicit fake_platform(journal& log) : log{log} {}

		void poll(std::vector<event>& events) override {
			log.push_back("poll");
			if (script.empty()) {
				events.push_back(quit_requested{});
				return;
			}
			for (int index = 0; index < live_redraws; ++index) {
				if (redraw) redraw();
			}
			auto batch = std::move(script.front());
			script.pop_front();
			events.insert(events.end(), batch.begin(), batch.end());
		}
		void idle() override { log.push_back("idle"); }
		void set_live_redraw(std::function<void()> callback) override { redraw = std::move(callback); }
	};

	struct fake_renderer : renderer {
		journal& log;
		explicit fake_renderer(journal& log) : log{log} {}

		void render() override { log.push_back("render"); }
		void wait_idle() override { log.push_back("wait_idle"); }

		std::vector<info_line> info{};
		std::uint64_t info_revision{};
		void describe(std::vector<info_line>& lines) const override {
			log.push_back("describe");
			lines.insert(lines.end(), info.begin(), info.end());
		}
		std::uint64_t revision() const noexcept override { return info_revision; }
	};

	struct fake_ui : ui {
		journal& log;
		explicit fake_ui(journal& log) : log{log} {}

		void new_frame(window const&, seconds dt) override {
			log.push_back(dt.count() >= 0 ? "new_frame" : "new_frame(negative dt)");
		}
		void end_frame() override { log.push_back("end_frame"); }
		void describe(std::vector<info_line>& lines) const override {
			lines.push_back({.source = "UI", .label = "Name", .value = "fake"});
		}
	};

	struct fake_stack : stack {
		fake_window window_{};
		fake_platform platform_;
		fake_renderer renderer_;
		fake_ui ui_;
		bool with_ui{true};

		explicit fake_stack(journal& log) : platform_{log}, renderer_{log}, ui_{log} {}

		app::platform& platform() noexcept override { return platform_; }
		app::window& window() noexcept override { return window_; }
		app::renderer& renderer() noexcept override { return renderer_; }
		app::ui* ui() noexcept override { return with_ui ? &ui_ : nullptr; }
	};
}  // namespace pyxel::app::testing
