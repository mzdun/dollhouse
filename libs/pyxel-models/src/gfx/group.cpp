// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/gfx/group.hpp>
#include <string>
#include <utility>
#include <variant>

namespace pyxel::gfx {
	group& group::set_light_transition(std::string name, interactions::transition rule) {
		for (auto& item : active_objects_) {
			auto* const light = std::get_if<interactions::light_active_object>(&item);
			if (light && light->name == name) {
				light->light = std::move(rule);
				return *this;
			}
		}

		active_objects_.emplace_back(
		    interactions::light_active_object{.name = std::move(name), .light = std::move(rule)});
		return *this;
	}

	void group::output(sink& out, glm::dmat4 const& current) const {
		auto const matrix = current_matrix(current);

		if (name_.empty()) {
			// no node of its own: the matrix keeps composing, to be baked
			// into the vertices of the nearest named group
			auto const guard = out.scope({}, material_);
			for (auto const& child : children_)
				child->output(out, matrix);
			return;
		}

		// a node of its own, with everything since the nearest named group;
		// the children start over, relative to the new node
		auto const guard = out.scope(name_, material_, matrix);
		for (auto const& child : children_)
			child->output(out, identity_matrix);
	}
}  // namespace pyxel::gfx
