// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/gfx/solid.hpp>
#include <utility>

namespace pyxel::gfx {
	solid::solid(std::vector<offset> vertices, std::vector<std::vector<size_t>> faces)
	    : vertices_{std::move(vertices)}, faces_{std::move(faces)} {
		// the first of equal vertices is the one add_vert returns
		for (size_t index = 0; index < vertices_.size(); ++index)
			lookup_.try_emplace(vertices_[index], index);
	}

	void solid::reserve(size_t vertex_count, size_t face_count) {
		vertices_.reserve(vertex_count);
		faces_.reserve(face_count);
	}

	size_t solid::add_vert(offset const& v) {
		auto const [it, inserted] = lookup_.try_emplace(v, vertices_.size());
		if (inserted) vertices_.push_back(v);
		return it->second;
	}

	void solid::add_face(std::vector<size_t> const& indexes) { faces_.push_back(indexes); }

	solid solid::transform(glm::dmat4 const& matrix) const {
		std::vector<offset> vertices{};
		vertices.reserve(vertices_.size());
		for (auto const& vertex : vertices_)
			vertices.push_back(vertex.apply(matrix));

		auto faces = faces_;
		if (glm::determinant(glm::dmat3{matrix}) < 0) {
			for (auto& face : faces)
				std::ranges::reverse(face);
		}

		return {std::move(vertices), std::move(faces)};
	}

	bool solid::is_valid() const noexcept {
		auto const count = vertices_.size();
		return std::ranges::all_of(faces_, [count](auto const& face) {
			return std::ranges::all_of(face, [count](size_t index) { return index < count; });
		});
	}
}  // namespace pyxel::gfx
