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

	void group::add_active_objects(sink& out) const {
		// the current node is the one of this group, if it is named, or of
		// the nearest named group above it
		for (auto const& declaration : active_objects_)
			out.add_active_object(declaration);
	}

	void group::output(sink& out, glm::dmat4 const& current) const {
		auto const matrix = current_matrix(current);

		if (name_.empty()) {
			// no node of its own: the matrix keeps composing, to be baked
			// into the vertices of the nearest named group
			auto const guard = out.scope({}, material_);
			add_active_objects(out);
			for (auto const& child : children_)
				child->output(out, matrix);
			return;
		}

		// a node of its own, with everything since the nearest named group;
		// the children start over, relative to the new node
		auto const guard = out.scope(name_, material_, matrix);
		add_active_objects(out);
		for (auto const& child : children_)
			child->output(out, identity_matrix);
	}
}  // namespace pyxel::gfx
