// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/object.hpp>
#include <utility>

namespace pyxel::gfx {
	// object holding a light, so it can be added to a group; the light is
	// kept as a copy, and handed to the sink with the matrix placing it
	struct light_object : object {
		explicit light_object(gfx::light value) : light_{std::move(value)} {}

		gfx::light const& light() const noexcept { return light_; }

		void output(sink& out, glm::dmat4 const& parent) const override { out.add_light(light_, parent); }

	private:
		gfx::light light_;
	};
}  // namespace pyxel::gfx
