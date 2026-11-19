// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <cmath>
#include <pyxel/gfx/solid.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// NaN breaks the ordering of the lookup map, and infinite
		// coordinates are no geometry
		void require_finite(offset const& v) {
			if (!std::isfinite(v.x.count()) || !std::isfinite(v.y.count()) || !std::isfinite(v.z.count()))
				throw std::invalid_argument("solid: vertex coordinates must be finite");
		}
	}  // namespace

	solid::solid(std::vector<offset> vertices, std::vector<std::vector<size_t>> faces)
	    : vertices_{std::move(vertices)}, faces_{std::move(faces)} {
		for (auto const& vertex : vertices_)
			require_finite(vertex);
		// the first of equal vertices is the one add_vert returns
		for (size_t index = 0; index < vertices_.size(); ++index)
			lookup_.try_emplace(vertices_[index], index);
	}

	void solid::reserve(size_t vertex_count, size_t face_count) {
		vertices_.reserve(vertex_count);
		faces_.reserve(face_count);
	}

	size_t solid::add_vert(offset const& v) {
		require_finite(v);
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
