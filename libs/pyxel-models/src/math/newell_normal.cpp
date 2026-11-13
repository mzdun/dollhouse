// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <pyxel/math/newell_normal.hpp>

namespace pyxel {
	std::optional<glm::dvec3> newell_normal(std::span<glm::vec3 const> vertices,
	                                        std::span<size_t const> face,
	                                        double tolerance) {
		if (face.empty()) return std::nullopt;

		// relative to the first vertex, so faces far from the origin keep
		// their precision
		glm::dvec3 const origin = vertices[face[0]];

		glm::dvec3 result{};
		double longest_squared = 0;
		for (size_t index = 0; index < face.size(); ++index) {
			auto const current = glm::dvec3{vertices[face[index]]} - origin;
			auto const next = glm::dvec3{vertices[face[(index + 1) % face.size()]]} - origin;

			auto const edge = next - current;
			longest_squared = std::max(longest_squared, glm::dot(edge, edge));

			result.x += (current.y - next.y) * (current.z + next.z);
			result.y += (current.z - next.z) * (current.x + next.x);
			result.z += (current.x - next.x) * (current.y + next.y);
		}

		// |result| is twice the area of the face
		auto const length = glm::length(result);
		if (!(length > tolerance * longest_squared) || !(length > 0)) return std::nullopt;
		return result / length;
	}
}  // namespace pyxel
