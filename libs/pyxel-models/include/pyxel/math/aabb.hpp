// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <limits>
#include <pyxel/math/glm.hpp>

namespace pyxel {
	// Axis-aligned bounding box; empty until the first point is added
	struct AABB {
		static constexpr float inf = std::numeric_limits<float>::infinity();

		glm::vec3 min{inf, inf, inf};
		glm::vec3 max{-inf, -inf, -inf};

		bool empty() const noexcept { return min.x > max.x || min.y > max.y || min.z > max.z; }

		void add(float x, float y, float z) noexcept {
			if (min.x > x) min.x = x;
			if (min.y > y) min.y = y;
			if (min.z > z) min.z = z;

			if (max.x < x) max.x = x;
			if (max.y < y) max.y = y;
			if (max.z < z) max.z = z;
		}

		AABB& operator+=(AABB const& rhs) noexcept {
			if (rhs.empty()) return *this;

			add(rhs.min.x, rhs.min.y, rhs.min.z);
			add(rhs.max.x, rhs.max.y, rhs.max.z);

			return *this;
		}

		// both are zero for an empty box
		glm::vec3 mid() const noexcept { return empty() ? glm::vec3{} : (min + max) / 2.0f; }
		glm::vec3 size() const noexcept { return empty() ? glm::vec3{} : max - min; }
	};
}  // namespace pyxel
