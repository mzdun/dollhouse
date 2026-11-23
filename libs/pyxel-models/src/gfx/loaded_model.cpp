// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/gfx/loaded_model.hpp>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	std::optional<size_t> loaded_model::find_node(std::string_view name) const noexcept {
		auto const it = std::ranges::find(nodes, name, &node::name);
		if (it == nodes.end()) return std::nullopt;
		return static_cast<size_t>(it - nodes.begin());
	}

	std::vector<glm::mat4> loaded_model::world_matrices(std::span<glm::mat4 const> moves) const {
		auto const moved = moves.size() == nodes.size();
		std::vector<glm::mat4> result{};
		result.reserve(nodes.size());
		for (size_t index = 0; index < nodes.size(); ++index) {
			auto const& current = nodes[index];
			auto local = moved ? current.local * moves[index] : current.local;
			// a parent comes before its children, so its matrix is ready
			if (current.parent && *current.parent < index) local = result[*current.parent] * local;
			result.push_back(local);
		}
		return result;
	}

	AABB loaded_model::bounds(std::span<glm::mat4 const> moves) const {
		auto const world = world_matrices(moves);
		AABB result{};
		for (size_t index = 0; index < nodes.size(); ++index) {
			for (auto const mesh_index : nodes[index].meshes) {
				if (mesh_index >= meshes.size()) continue;
				for (auto const& position : meshes[mesh_index].positions) {
					auto const point = world[index] * glm::vec4{position, 1.0f};
					result.add(point.x, point.y, point.z);
				}
			}
		}
		return result;
	}

	void loaded_model::resolve(interactions::loaded_objects loaded) {
		objects.clear();
		objects.reserve(loaded.size());
		for (auto& object : loaded) {
			active_object item{.object = std::move(object)};
			if (item.object.kind == interactions::object_kind::light) {
				auto const it = std::ranges::find(lights, item.object.name, &light::name);
				if (it != lights.end()) item.light = static_cast<size_t>(it - lights.begin());
			} else {
				item.nodes.reserve(item.object.groups.size());
				for (auto const& group : item.object.groups)
					item.nodes.push_back(find_node(group.group));
			}
			objects.push_back(std::move(item));
		}
	}
}  // namespace pyxel::gfx
