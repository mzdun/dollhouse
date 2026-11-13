// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <optional>
#include <pyxel/gfx/space.hpp>
#include <pyxel/math/glm.hpp>

// Building blocks of transformation matrices.
//
// Each function returns a copy of `matrix` with one more transformation,
// applied after the ones already in it, so a chain reads in the order the
// transformations happen:
//
//     rotate_z(translate(identity, 1_m), 90_deg)
//
// moves by 1 m along x first, then rotates around the z axis, ending up on
// the y axis. Pivots are given in the coordinates the rotation happens in,
// that is after the earlier transformations.
namespace pyxel::gfx {
	inline glm::dmat4 const identity_matrix{1.0};

	glm::dmat4 translate(glm::dmat4 const& matrix, offset const& delta);
	glm::dmat4 translate(glm::dmat4 const& matrix, length x, length y = {}, length z = {});

	// missing factors are equal to `x`, so `scale(m, 2)` scales uniformly
	glm::dmat4 scale(glm::dmat4 const& matrix,
	                 double x,
	                 std::optional<double> y = std::nullopt,
	                 std::optional<double> z = std::nullopt);

	// counter-clockwise, when looking from the tip of `axis` towards the
	// pivot; the axis needs not be normalized, but must not be zero, or
	// std::invalid_argument is thrown
	glm::dmat4 rotate(glm::dmat4 const& matrix, angle by, glm::dvec3 const& axis, offset const& pivot = {});

	glm::dmat4 rotate_x(glm::dmat4 const& matrix, angle by, offset const& pivot = {});
	glm::dmat4 rotate_y(glm::dmat4 const& matrix, angle by, offset const& pivot = {});
	glm::dmat4 rotate_z(glm::dmat4 const& matrix, angle by, offset const& pivot = {});
}  // namespace pyxel::gfx
