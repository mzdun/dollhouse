// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <cmath>
#include <pyxel/camera.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#include <glm/gtc/matrix_transform.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace pyxel {
	math::radians doom_fov_y() noexcept { return math::radians{2.0 * std::atan(0.75)}; }

	glm::mat4 camera::view() const noexcept { return glm::lookAtRH(eye, target, up); }

	glm::mat4 camera::projection(float aspect) const noexcept {
		return glm::perspectiveRH_ZO(static_cast<float>(fov_y.count()), aspect, near_plane, far_plane);
	}

	camera camera::framing(AABB const& box, float aspect, math::radians fov_y) noexcept {
		// The bounding sphere of the box is inside the frustum, if it is inside
		// the narrower of the two cones the frustum holds: the vertical one,
		// and the horizontal one, narrower in a portrait image.
		auto radius = glm::length(box.size()) / 2.0f;
		if (radius <= 0.0f) radius = 1.0f;

		auto const half_y = fov_y.count() / 2.0;
		auto const half_x = std::atan(std::tan(half_y) * static_cast<double>(aspect));
		auto const half = static_cast<float>(std::min(half_y, half_x));
		auto const distance = radius / std::sin(half);

		auto const middle = box.mid();
		auto const diagonal = glm::normalize(glm::vec3{1.0f, 1.0f, 1.0f});

		camera result{};
		result.eye = middle + diagonal * distance;
		result.target = middle;
		result.fov_y = fov_y;
		// distance > radius, as sin(half) < 1; a little room on both sides,
		// so the box does not touch the planes
		result.near_plane = std::max((distance - radius) * 0.99f, distance * 1e-3f);
		result.far_plane = (distance + radius) * 1.01f;
		return result;
	}
}  // namespace pyxel
