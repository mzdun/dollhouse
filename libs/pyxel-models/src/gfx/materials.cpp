// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <format>
#include <pyxel/gfx/materials.hpp>

namespace pyxel::gfx {
	std::string materials::register_paint(std::string const& name, paint_opts const& opts) {
		return register_material(name, opts);
	}

	materials::material_t materials::lookup(std::string const& name) const {
		auto const it = materials_.find(name);
		if (it == materials_.end()) return {};
		return it->second;
	}

	std::string materials::register_material(std::string const& name, material_t const& material) {
		auto actual = name.empty() ? next_name() : name;
		materials_[actual] = material;
		return actual;
	}

	std::string materials::next_name() {
		auto const index = next_material_++;
		if (!index) return "Material";
		return std::format("Material.{:03}", index);
	}
}  // namespace pyxel::gfx
