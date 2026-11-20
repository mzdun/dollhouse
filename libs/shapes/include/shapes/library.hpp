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
#include <string_view>
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

	// The models and the configuration may be used from many threads.
	// build_models() runs the models without holding any lock, so a model
	// may register other models. The paints are not synchronized, register
	// them before building or from the models. Changing the configuration
	// (exporter, output directory, export listener) or clearing the models
	// is refused while building; a build uses the configuration from the
	// moment it started.
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
		std::string exporter_id() const;
		// throws std::invalid_argument for ids not listed in exporters() and
		// std::logic_error, if called while building
		void set_exporter_id(std::string id);

		// where build_models() writes the files; current directory by default
		std::filesystem::path output_directory() const;
		// throws std::logic_error, if called while building
		void set_output_directory(std::filesystem::path dir);

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
		// writes, e.g. by the command line tool to list them; called on the
		// building thread, without any lock held
		using export_listener = std::function<void(std::string const& name, std::filesystem::path const& path)>;
		// throws std::logic_error, if called while building
		void set_export_listener(export_listener listener);

	private:
		// the configuration of one build, copied when it starts
		struct build_config {
			std::string exporter_id{};
			std::filesystem::path output_directory{};
			export_listener listener{};
		};

		static std::unique_ptr<built_model> build_model(std::string const& name,
		                                                model_fn const&,
		                                                build_config const& config);
		std::filesystem::path export_model(built_model const& model, build_config const& config) const;
		// throws std::logic_error naming `what`, if building; call with
		// models_mutex_ held
		void refuse_while_building(std::string_view what) const;

		library* previous_{nullptr};
		std::atomic<bool> building_{false};
		// guards models_ (including model_entry::ran), closed_ and the
		// configuration; the model nodes stay put while building, as
		// clear_models() is refused and close() leaves them to the build then
		mutable std::mutex models_mutex_{};
		bool closed_{false};
		model_map models_{};
		std::string exporter_id_{};
		std::filesystem::path output_directory_{"."};
		export_listener export_listener_{};
		pyxel::gfx::materials material_repo_{};
	};
}  // namespace shapes
