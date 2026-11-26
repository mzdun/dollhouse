// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/math/aabb.hpp>
#include <pyxel/math/glm.hpp>
#include <pyxel/math/value.hpp>

namespace pyxel {
	// Doom shows 90° across a 4:3 screen, which is 2·atan(3/4), about 73.74°,
	// from the bottom of the screen to the top
	math::radians doom_fov_y() noexcept;

	// A perspective camera, in the right-handed world of glTF 2 (+Y up). The
	// projection maps the depth to [0, 1], as SDL GPU expects it.
	struct camera {
		glm::vec3 eye{0.0f, 0.0f, 1.0f};
		glm::vec3 target{};
		glm::vec3 up{0.0f, 1.0f, 0.0f};
		// from the bottom of the image to the top
		math::radians fov_y{doom_fov_y()};
		float near_plane{0.1f};
		float far_plane{100.0f};

		glm::mat4 view() const noexcept;
		// aspect is the width over the height of the image
		glm::mat4 projection(float aspect) const noexcept;

		// Looks from the (+X, +Y, +Z) side down the diagonal of a cube, at the
		// middle of the box, from just far enough to keep the whole box inside
		// the frustum, with the near and far planes around the box. An empty
		// or a single-point box is seen as a sphere of 1 m around its middle.
		static camera framing(AABB const& box, float aspect, math::radians fov_y = doom_fov_y()) noexcept;
	};
}  // namespace pyxel
