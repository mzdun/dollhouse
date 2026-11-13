// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/gfx/group.hpp>

namespace pyxel::gfx {
	void group::output(sink& out, glm::dmat4 const& current) const {
		auto const matrix = current_matrix(current);
		auto const guard = out.scope(name_, material_);

		for (auto const& child : children_)
			child->output(out, matrix);
	}
}  // namespace pyxel::gfx
