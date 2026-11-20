// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/materials.hpp>
#include <string>
#include <utility>
#include <vector>

namespace shapes {
	// fills the root group given by the host; returns the id of the exporter
	// to use for the model, or nothing for the default one
	using model_fn = std::function<std::optional<std::string>(pyxel::gfx::group& root)>;
	// a registered model; `ran` is set, once build_models() wrote it, so the
	// models added later, e.g. by the next script, are built on their own
	struct model_entry {
		model_fn fn{};
		bool ran{false};
	};
	using model_map = std::map<std::string, model_entry, std::less<>>;

	// The model registry (add_model, models, clear_models, close,
	// build_models) may be used from many threads; build_models runs the
	// models without holding the registry lock, so the models may register
	// other models. The configuration (exporter, output directory, paints,
	// export listener) is not synchronized, set it up before building.
	class library {
	public:
		static library& python_instance();

		struct built_model {
			std::string name{};
			std::string exporter{};
			pyxel::gfx::group root{};
		};

		library();  // do not bind
		~library();

		library(library const&) = delete;
		library& operator=(library const&) = delete;
		library(library&&) = delete;
		library& operator=(library&&) = delete;

		// ids of the Assimp export formats, e.g. "glb2" or "obj"
		static std::vector<std::string> const& exporters();

		// used for the models returning no exporter id; defaults to glTF 2
		// binary ("glb2"), if available, else glTF 1 binary ("glb")
		std::string const& exporter_id() const noexcept { return exporter_id_; }
		// throws std::invalid_argument for ids not listed in exporters()
		void set_exporter_id(std::string id);

		// where build_models() writes the files; current directory by default
		std::filesystem::path const& output_directory() const noexcept { return output_directory_; }
		void set_output_directory(std::filesystem::path dir) { output_directory_ = std::move(dir); }

		std::string register_paint(pyxel::gfx::materials::paint_opts const& opts) {
			return material_repo_.register_paint(opts);
		}
		std::string register_paint(std::string const& name, pyxel::gfx::materials::paint_opts const& opts) {
			return material_repo_.register_paint(name, opts);
		}

		// returns false, if a model with this name is already registered,
		// built or not; a model added while building waits for the next
		// build_models(); throws std::logic_error, once the library is closed
		bool add_model(std::string name, model_fn fn);
		// a copy of the registry; copies the model functions, so for Python
		// models, call it with the GIL held
		model_map models() const;
		// forgets every model, built or not; throws std::logic_error, if
		// called while building, see is_building()
		void clear_models();

		// forgets the models and refuses the new ones, for a host about to go
		// away: the models of the Python module hold Python callables, which
		// must be released while the interpreter is still alive; while
		// building, the models are forgotten, once the build is over
		void close() noexcept;
		bool is_closed() const;

		// builds the models, which did not run yet when it was called, and
		// marks them as ran; a model, which failed, stays unmarked; throws
		// std::logic_error, if called while building (e.g. from a model, or
		// from another thread)
		std::vector<std::filesystem::path> build_models();

		// true, while build_models() runs
		bool is_building() const noexcept { return building_.load(); }

		// called with the name and the path of every model build_models()
		// writes, e.g. by the command line tool to list them
		using export_listener = std::function<void(std::string const& name, std::filesystem::path const& path)>;
		void set_export_listener(export_listener listener) { export_listener_ = std::move(listener); }

	private:
		std::unique_ptr<built_model> build_model(std::string const& name, model_fn const&) const;
		std::filesystem::path export_model(built_model const& model) const;

		library* previous_{nullptr};
		std::atomic<bool> building_{false};
		// guards models_ (including model_entry::ran) and closed_; the model
		// nodes stay put while building, as clear_models() is refused and
		// close() leaves them to the build then
		mutable std::mutex models_mutex_{};
		bool closed_{false};
		model_map models_{};
		pyxel::gfx::materials material_repo_{};
		std::string exporter_id_{};
		std::filesystem::path output_directory_{"."};
		export_listener export_listener_{};
	};
}  // namespace shapes
