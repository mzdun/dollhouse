// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <format>
#include <memory>
#include <pyxel/base/str.hpp>
#include <pyxel/io/export.hpp>
#include <shapes/library.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shapes {
	namespace {
		using pyxel::io::export_format;
		using pyxel::io::export_formats;
		using pyxel::io::export_scene;

		library* current = nullptr;

		export_format const* find_format(std::string_view id) {
			auto const& formats = export_formats();
			auto const it = std::ranges::find(formats, id, &export_format::id);
			return it == formats.end() ? nullptr : &*it;
		}

		// path of the model file, relative to the output directory; both "/"
		// and "\\" separate the subdirectories, but the path must stay inside
		std::filesystem::path model_file(std::string_view name, std::string_view extension) {
			auto filename = std::format("{}.{}", name, extension);
			// on POSIX "\\" is a regular character, which lexically_normal()
			// leaves alone; both separators must mean the same everywhere
			std::ranges::replace(filename, '\\', '/');
			auto const path = std::filesystem::path{pyxel::as_u8v(filename)}.lexically_normal();
			// the last element always ends with ".{extension}", so it is never
			// removed by lexically_normal() and the path is never empty or "."
			if (path.has_root_path() || *path.begin() == "..") {
				throw std::invalid_argument(std::format("model `{}`: the name leaves the output directory", name));
			}
			return path;
		}

		std::string default_exporter() {
			for (auto const id : {"glb2", "glb"}) {
				if (find_format(id)) return id;
			}
			return {};
		}
	}  // namespace

	library& library::python_instance() {
		// there is always a library alive while scripts run
		return *current;
	}

	library::library() : previous_{current}, exporter_id_{default_exporter()} { current = this; }

	library::~library() {
		if (current == this) current = previous_;
	}

	std::vector<std::string> const& library::exporters() {
		static std::vector<std::string> const ids = [] {
			std::vector<std::string> result{};
			for (auto const& format : export_formats())
				result.push_back(format.id);
			return result;
		}();
		return ids;
	}

	void library::refuse_while_building(std::string_view what) const {
		// checked under models_mutex_: a build, which starts later, sees the
		// change in its snapshot; one, which started earlier, keeps us out
		if (building_.load()) throw std::logic_error(std::format("library.{} while building the models", what));
	}

	std::string library::exporter_id() const {
		std::lock_guard const lock{models_mutex_};
		return exporter_id_;
	}

	void library::set_exporter_id(std::string id) {
		if (!find_format(id)) throw std::invalid_argument(std::format("unknown exporter `{}`", id));
		std::lock_guard const lock{models_mutex_};
		refuse_while_building("exporter_id: cannot change the exporter");
		exporter_id_ = std::move(id);
	}

	std::filesystem::path library::output_directory() const {
		std::lock_guard const lock{models_mutex_};
		return output_directory_;
	}

	void library::set_output_directory(std::filesystem::path dir) {
		std::lock_guard const lock{models_mutex_};
		refuse_while_building("output_directory: cannot change the output directory");
		output_directory_ = std::move(dir);
	}

	void library::set_export_listener(export_listener listener) {
		std::lock_guard const lock{models_mutex_};
		refuse_while_building("set_export_listener: cannot change the listener");
		export_listener_ = std::move(listener);
	}

	bool library::add_model(std::string name, model_fn fn) {
		std::lock_guard const lock{models_mutex_};
		if (closed_) {
			throw std::logic_error(std::format("model `{}`: the library is closed", name));
		}
		// exceptional early exit on output directory escape
		(void)model_file(name, "ext");
		return models_.try_emplace(std::move(name), model_entry{.fn = std::move(fn)}).second;
	}

	model_map library::models() const {
		std::lock_guard const lock{models_mutex_};
		return models_;
	}

	void library::clear_models() {
		std::lock_guard const lock{models_mutex_};
		refuse_while_building("clear_models: cannot clear the models");
		models_.clear();
	}

	void library::close() noexcept {
		std::lock_guard const lock{models_mutex_};
		closed_ = true;
		// a running build still walks the models; it forgets them, when done
		if (!building_.load()) models_.clear();
	}

	bool library::is_closed() const {
		std::lock_guard const lock{models_mutex_};
		return closed_;
	}

	std::vector<std::filesystem::path> library::build_models() {
		if (building_.exchange(true)) throw std::logic_error("library.build_models: already building the models");
		struct building_guard {
			library& self;
			~building_guard() {
				std::lock_guard const lock{self.models_mutex_};
				// close() was called while building and left the models to us
				if (self.closed_) self.models_.clear();
				self.building_.store(false);
			}
		} const guard{*this};

		// the models to build in this run; the map nodes do not move on
		// insertion, and nothing erases them while building_ is set, so the
		// iterators outlive the lock. Keys and functions are never written
		// after insertion, so they are read without the lock below
		std::vector<model_map::iterator> pending{};
		build_config config{};
		{
			std::lock_guard const lock{models_mutex_};
			config = {.exporter_id = exporter_id_, .output_directory = output_directory_, .listener = export_listener_};
			pending.reserve(models_.size());
			for (auto it = models_.begin(); it != models_.end(); ++it) {
				if (!it->second.ran) pending.push_back(it);
			}
		}

		std::vector<std::filesystem::path> written{};
		written.reserve(pending.size());
		for (auto const it : pending) {
			written.push_back(export_model(*build_model(it->first, it->second.fn, config), config));
			{
				std::lock_guard const lock{models_mutex_};
				it->second.ran = true;
			}
			if (config.listener) config.listener(it->first, written.back());
		}
		return written;
	}

	std::unique_ptr<library::built_model> library::build_model(std::string const& name,
	                                                           model_fn const& fn,
	                                                           build_config const& config) {
		auto model = std::make_unique<built_model>();
		model->name = name;
		model->exporter = fn(model->root).value_or(config.exporter_id);
		if (!find_format(model->exporter)) {
			throw std::invalid_argument(std::format("model `{}`: unknown exporter `{}`", name, model->exporter));
		}
		return model;
	}

	std::filesystem::path library::export_model(built_model const& model, build_config const& config) const {
		auto const* format = find_format(model.exporter);
		auto path = config.output_directory / model_file(model.name, format->extension);
		std::filesystem::create_directories(path.parent_path());

		pyxel::gfx::sink out{};
		model.root.output(out, glm::dmat4{1});
		export_scene(model.name, out, material_repo_, model.exporter, path);
		return path;
	}
}  // namespace shapes
