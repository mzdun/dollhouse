// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <memory>
#include <pyxel/app/application.hpp>
#include <pyxel/camera.hpp>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/io/interactions.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "loader.hpp"
#include "options.hpp"
#include "panels/log.hpp"

namespace inspector {
	class application : public pyxel::app::application {
	public:
		std::string_view name() const noexcept override;
		std::string title() const override;

		bool init(::args::args_view const& args) override;
		pyxel::app::driver_hints driver() const override { return options_.driver; }
		std::unique_ptr<pyxel::app::stack> create_stack(pyxel::app::window_options const& options,
		                                                pyxel::app::driver_hints const& hints) override;

		void setup() override;
		void update(pyxel::app::seconds) override;
		void draw_ui() override;
		void shutdown() override;

		// for the panels
		bool verbose() const noexcept { return options_.verbose; }
		bool loading() const noexcept { return loader_.loading(); }
		std::string const& error() const noexcept { return error_; }
		std::vector<pyxel::io::interactions::note> const& notes() const noexcept { return notes_; }
		pyxel::camera const& camera() const noexcept { return camera_; }

	private:
		void draw_menu();
		void apply(load_result result);
		// immovable for now: looks at the whole model, again after every
		// reset and every new size of the window
		void frame_camera();
		void update_title();

		options options_{};
		pyxel::gfx::loaded_model model_{};
		std::vector<pyxel::io::interactions::note> notes_{};
		std::string error_{};
		pyxel::camera camera_{};
		pyxel::app::extent frame_size_{};
		// the last stamp of loaded::resets, which started the model over
		unsigned resets_applied_{0};
		bool show_demo_{false};
		panels::log log_{};
		// last: its thread stops before anything else here is gone
		loader loader_{};
	};
}  // namespace inspector
