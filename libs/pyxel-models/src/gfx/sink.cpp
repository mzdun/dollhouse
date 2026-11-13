// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/gfx/sink.hpp>

namespace pyxel::gfx {
	void sink::add_solid(solid const& solid, glm::dmat4 const& matrix) {
		static std::string const no_material{};
		auto const& material = materials_.empty() ? no_material : materials_.back();
		solids_[material][names_].add_solid(solid, matrix);
	}

	void sink::push_object_name(std::string const& name) { names_.emplace_back(name); }

	void sink::pop_object_name() {
		if (!names_.empty()) names_.pop_back();
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
