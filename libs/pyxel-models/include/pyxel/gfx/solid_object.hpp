// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/object.hpp>
#include <pyxel/gfx/solid.hpp>
#include <utility>

namespace pyxel::gfx {
	// object holding a ready-made solid, e.g. a result of the operations, so
	// it can be added to a group; the solid is kept as a copy
	struct solid_object : object {
		explicit solid_object(gfx::solid shape) : shape_{std::move(shape)} {}

		gfx::solid const& shape() const noexcept { return shape_; }

		void output(sink& out, glm::dmat4 const& parent) const override { out.add_solid(shape_, parent); }

	private:
		gfx::solid shape_;
	};
}  // namespace pyxel::gfx
