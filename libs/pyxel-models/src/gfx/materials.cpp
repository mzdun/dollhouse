// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <format>
#include <pyxel/gfx/materials.hpp>
#include <stdexcept>
#include <string>

namespace pyxel::gfx {
	std::string materials::register_paint(std::string const& name, paint_opts const& opts) {
		// NaN fails the comparisons; paints are compared with <=>
		if (!(opts.roughness >= 0.0f && opts.roughness <= 1.0f))
			throw std::invalid_argument(std::format("paint: roughness must be within [0, 1], not {}", opts.roughness));
		if (!(opts.metallic >= 0.0f && opts.metallic <= 1.0f))
			throw std::invalid_argument(std::format("paint: metallic must be within [0, 1], not {}", opts.metallic));
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
