// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <optional>
#include <pyxel/math/glm.hpp>
#include <span>

namespace pyxel {
	// Default for newell_normal: a little above the relative precision of
	// float vertices, so rounding noise is not mistaken for a real surface.
	inline constexpr double newell_tolerance = 1e-6;

	// Unit normal of a polygon, counter-clockwise faces point towards the
	// viewer.
	//
	// Empty for degenerate faces: a face is degenerate, when twice its area is
	// not larger than `tolerance` times its longest edge squared. For a
	// triangle, that is its height relative to its longest edge, so the test
	// does not depend on the size of the face, nor on its distance from the
	// origin. A tolerance of zero only rejects faces with no area at all.
	std::optional<glm::dvec3> newell_normal(std::span<glm::vec3 const> vertices,
	                                        std::span<size_t const> face,
	                                        double tolerance = newell_tolerance);
}  // namespace pyxel
