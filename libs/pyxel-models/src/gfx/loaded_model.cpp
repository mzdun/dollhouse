// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/gfx/transform.hpp>
#include <utility>
#include <variant>
#include <vector>

namespace pyxel::gfx {
	namespace {
		namespace ia = interactions;

		template <class... Ts>
		struct overloaded : Ts... {
			using Ts::operator()...;
		};

		offset along(axis on, length by) noexcept {
			offset result{};
			result[on] = by;
			return result;
		}

		glm::dvec3 unit(axis on) noexcept {
			glm::dvec3 result{};
			result[static_cast<glm::length_t>(on)] = 1.0;
			return result;
		}

		// the matrix of a single channel, on its own
		glm::dmat4 matrix_of(ia::group_transformation const& change) {
			return std::visit(
			    overloaded{
			        [](ia::translate_axis const& item) { return translate(identity_matrix, along(item.on, item.by)); },
			        [](ia::translate_all const& item) { return translate(identity_matrix, item.by); },
			        [](ia::rotate_axis const& item) { return rotate(identity_matrix, item.by, unit(item.around)); },
			        // Rx · Ry · Rz, as glm::eulerAngleXYZ builds it
			        [](ia::rotate_all const& item) {
				        return rotate_x(identity_matrix, item.x) * rotate_y(identity_matrix, item.y) *
				               rotate_z(identity_matrix, item.z);
			        },
			        [](ia::scale_axis const& item) {
				        glm::dvec3 factors{1.0};
				        factors[static_cast<glm::length_t>(item.along)] = item.by;
				        return scale(identity_matrix, factors.x, factors.y, factors.z);
			        },
			        [](ia::scale_all const& item) { return scale(identity_matrix, item.x, item.y, item.z); },
			    },
			    change);
		}
	}  // namespace

	glm::dmat4 move_of(std::span<interactions::group_transformation const> values) {
		auto result = identity_matrix;
		for (auto const& change : values)
			result = result * matrix_of(change);
		return result;
	}

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

	std::vector<glm::dmat4> loaded_model::moves(std::span<std::optional<size_t> const> states) const {
		std::vector<glm::dmat4> result(nodes.size(), identity_matrix);
		for (size_t index = 0; index < objects.size() && index < states.size(); ++index) {
			auto const& object = objects[index].object;
			auto const& object_nodes = objects[index].nodes;
			auto const state = states[index];
			if (!state || *state >= object.states.size()) continue;

			auto const& values = object.states[*state].values.groups;
			for (size_t group = 0; group < values.size() && group < object_nodes.size(); ++group) {
				auto const node = object_nodes[group];
				if (!node || *node >= result.size()) continue;
				result[*node] = result[*node] * move_of(values[group]);
			}
		}
		return result;
	}

	std::vector<std::optional<size_t>> loaded_model::start_states() const {
		std::vector<std::optional<size_t>> result{};
		result.reserve(objects.size());
		for (auto const& active : objects)
			result.push_back(active.object.start);
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
