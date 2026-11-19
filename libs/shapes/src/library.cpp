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

	void library::set_exporter_id(std::string id) {
		if (!find_format(id)) throw std::invalid_argument(std::format("unknown exporter `{}`", id));
		exporter_id_ = std::move(id);
	}

	bool library::add_model(std::string name, model_fn fn) {
		if (closed_) {
			throw std::logic_error(std::format("model `{}`: the library is closed", name));
		}
		// exceptional early exit on output directory escape
		(void)model_file(name, "ext");
		return models_.try_emplace(std::move(name), model_entry{.fn = std::move(fn)}).second;
	}

	std::vector<std::filesystem::path> library::build_models() {
		if (building_) throw std::logic_error("library.build_models: already building the models");
		struct building_guard {
			bool& flag;
			explicit building_guard(bool& flag) : flag{flag} { flag = true; }
			~building_guard() { flag = false; }
		} const guard{building_};

		std::vector<std::filesystem::path> written{};
		for (auto& [name, entry] : models_) {
			if (entry.ran) continue;
			written.push_back(export_model(*build_model(name, entry.fn)));
			entry.ran = true;
			if (export_listener_) export_listener_(name, written.back());
		}
		return written;
	}

	std::unique_ptr<library::built_model> library::build_model(std::string const& name, model_fn const& fn) const {
		auto model = std::make_unique<built_model>();
		model->name = name;
		model->exporter = fn(model->root).value_or(exporter_id_);
		if (!find_format(model->exporter)) {
			throw std::invalid_argument(std::format("model `{}`: unknown exporter `{}`", name, model->exporter));
		}
		return model;
	}

	std::filesystem::path library::export_model(built_model const& model) const {
		auto const* format = find_format(model.exporter);
		auto path = output_directory_ / model_file(model.name, format->extension);
		std::filesystem::create_directories(path.parent_path());

		pyxel::gfx::sink out{};
		model.root.output(out, glm::dmat4{1});
		export_scene(model.name, out, material_repo_, model.exporter, path);
		return path;
	}
}  // namespace shapes
