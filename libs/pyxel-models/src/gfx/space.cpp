// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/gfx/space.hpp>

namespace pyxel::gfx {
	offset offset::apply(glm::dmat4 const& transform) const noexcept {
		auto const result = transform * glm::dvec4{to_glm(), 1.0};
		return from(glm::dvec3{result} / result.w);
	}

	offset offset::apply_direction(glm::dmat4 const& transform) const noexcept {
		return from(glm::dvec3{transform * glm::dvec4{to_glm(), 0.0}});
	}
}  // namespace pyxel::gfx
