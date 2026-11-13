// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <format>
#include <shapes/library.hpp>
#include <stdexcept>
#include <utility>
#include "export.hpp"

namespace shapes {
	namespace {
		library* current = nullptr;

		export_format const* find_format(std::string_view id) {
			auto const& formats = export_formats();
			auto const it = std::ranges::find(formats, id, &export_format::id);
			return it == formats.end() ? nullptr : &*it;
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
		auto path = output_directory_ / std::format("{}.{}", model.name, format->extension);
		std::filesystem::create_directories(output_directory_);

		pyxel::gfx::sink out{};
		model.root.output(out, glm::dmat4{1});
		export_scene(model.name, out, material_repo_, model.exporter, path);
		return path;
	}
}  // namespace shapes
