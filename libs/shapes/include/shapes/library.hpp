// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/materials.hpp>
#include <string>
#include <vector>

namespace shapes {
	// fills the root group given by the host; returns the id of the exporter
	// to use for the model, or nothing for the default one
	using model_fn = std::function<std::optional<std::string>(pyxel::gfx::group& root)>;
	using model_map = std::map<std::string, model_fn, std::less<>>;

	// This class is not re-entrant, do not use in parallel
	class library {
	public:
		static library& python_instance();

		library();  // do not bind
		~library();

		library(library const&) = delete;
		library& operator=(library const&) = delete;
		library(library&&) = delete;
		library& operator=(library&&) = delete;

		std::string register_paint(pyxel::gfx::materials::paint_opts const& opts) {
			return material_repo_.register_paint(opts);
		}
		std::string register_paint(std::string const& name, pyxel::gfx::materials::paint_opts const& opts) {
			return material_repo_.register_paint(name, opts);
		}

		// returns false, if a model with this name is already registered
		bool add_model(std::string name, model_fn fn);
		model_map const& models() const noexcept { return models_; }
		void clear_models() noexcept { models_.clear(); }

	private:
		library* previous_{nullptr};
		model_map models_{};
		pyxel::gfx::materials material_repo_{};
	};
}  // namespace shapes
