// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/gfx/sink.hpp>
#include <stdexcept>
#include <utility>

namespace pyxel::gfx {
	sink::sink() : nodes_(1) {}

	void sink::add_solid(solid const& solid, glm::dmat4 const& matrix) {
		static std::string const no_material{};
		auto const& material = materials_.empty() ? no_material : materials_.back();
		nodes_[current_].meshes[material].add_solid(solid, matrix);
	}

	void sink::add_light(gfx::light const& light, glm::dmat4 const& matrix) {
		nodes_[current_].lights.push_back({.light = light, .matrix = matrix});
	}

	void sink::push_node(std::string name, glm::dmat4 const& local) {
		auto const id = nodes_.size();
		nodes_.push_back({.parent = current_, .name = std::move(name), .local = local});
		nodes_[current_].children.push_back(id);
		current_ = id;
	}

	void sink::pop_node() {
		if (auto const parent = nodes_[current_].parent) current_ = *parent;
	}

	glm::dmat4 sink::global_matrix(node_id id) const {
		glm::dmat4 result{1.0};
		for (std::optional<node_id> next = id; next; next = nodes_.at(*next).parent)
			result = nodes_[*next].local * result;
		return result;
	}

	sink::scope_context::scope_context(sink& out,
	                                   std::string const& name,
	                                   std::string const& material,
	                                   std::optional<glm::dmat4> const& matrix)
	    : out_{out}, has_name_{!name.empty()}, has_material_{!material.empty()} {
		if (matrix && !has_name_) {
			throw std::invalid_argument("sink.scope: a matrix needs a name, to open a node for it");
		}
		if (has_name_) out_.push_node(name, matrix.value_or(glm::dmat4{1.0}));
		if (has_material_) out_.push_material(material);
	}

	sink::scope_context::~scope_context() {
		if (has_material_) out_.pop_material();
		if (has_name_) out_.pop_node();
	}

	void sink::push_material(std::string const& name) { materials_.emplace_back(name); }

	void sink::pop_material() {
		if (!materials_.empty()) materials_.pop_back();
	}

	size_t sink::glm_solid::add_vertex(glm::dvec3 const& vertex) {
		auto const [it, inserted] = lookup_.try_emplace(vertex, vertices_.size());
		if (inserted) vertices_.push_back(vertex);
		return it->second;
	}

	void sink::glm_solid::add_solid(solid const& obj, glm::dmat4 const& matrix) {
		std::vector<size_t> remap{};
		remap.reserve(obj.vertices().size());
		for (auto const& vertex : obj.vertices())
			remap.push_back(add_vertex(vertex.apply(matrix).to_glm()));

		// a mirroring transform turns the faces inside out; reverse the
		// winding to keep them facing outwards
		auto const mirrored = glm::determinant(matrix) < 0.0;

		faces_.reserve(faces_.size() + obj.faces().size());
		for (auto const& src_face : obj.faces()) {
			face_t face{};
			face.reserve(src_face.size());
			auto valid = true;
			for (auto const index : src_face) {
				if (index >= remap.size()) {
					valid = false;
					break;
				}
				auto const mapped = remap[index];
				// merged vertices may produce repeated corners
				if (face.empty() || face.back() != mapped) face.push_back(mapped);
			}
			if (!valid) continue;

			while (face.size() > 1 && face.front() == face.back())
				face.pop_back();
			if (face.size() < 3) continue;

			if (mirrored) std::reverse(face.begin(), face.end());
			faces_.push_back(std::move(face));
		}
	}
}  // namespace pyxel::gfx
