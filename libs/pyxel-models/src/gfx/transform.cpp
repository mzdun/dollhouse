// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cmath>
#include <pyxel/gfx/transform.hpp>
#include <stdexcept>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#include <glm/gtc/matrix_transform.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace pyxel::gfx {
	// the new transformation goes on the left, so it is applied last
	glm::dmat4 translate(glm::dmat4 const& matrix, offset const& delta) {
		return glm::translate(identity_matrix, delta.to_glm()) * matrix;
	}

	glm::dmat4 translate(glm::dmat4 const& matrix, length x, length y, length z) {
		return translate(matrix, offset{x, y, z});
	}

	glm::dmat4 scale(glm::dmat4 const& matrix, double x, std::optional<double> y, std::optional<double> z) {
		glm::dvec3 const factors{x, y.value_or(x), z.value_or(x)};
		if (glm::any(glm::isnan(factors)) || glm::any(glm::isinf(factors)))
			throw std::invalid_argument("scale factors must be finite");
		return glm::scale(identity_matrix, factors) * matrix;
	}

	glm::dmat4 rotate(glm::dmat4 const& matrix, angle by, glm::dvec3 const& axis, offset const& pivot) {
		if (glm::any(glm::isnan(axis)) || glm::any(glm::isinf(axis)))
			throw std::invalid_argument("rotation axis must be finite");
		if (!(glm::length(axis) > 0.0)) throw std::invalid_argument("rotation axis must not be zero");
		if (!std::isfinite(by.count())) throw std::invalid_argument("rotation angle must be finite");

		auto const around = pivot.to_glm();
		auto const rotation = glm::translate(identity_matrix, around) * glm::rotate(identity_matrix, by.count(), axis) *
		                      glm::translate(identity_matrix, -around);
		return rotation * matrix;
	}

	glm::dmat4 rotate_x(glm::dmat4 const& matrix, angle by, offset const& pivot) {
		return rotate(matrix, by, {1, 0, 0}, pivot);
	}

	glm::dmat4 rotate_y(glm::dmat4 const& matrix, angle by, offset const& pivot) {
		return rotate(matrix, by, {0, 1, 0}, pivot);
	}

	glm::dmat4 rotate_z(glm::dmat4 const& matrix, angle by, offset const& pivot) {
		return rotate(matrix, by, {0, 0, 1}, pivot);
	}
}  // namespace pyxel::gfx
