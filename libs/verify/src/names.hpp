// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/loaded_object.hpp>
#include <pyxel/gfx/transition.hpp>
#include <string>
#include <string_view>

// The words the report uses for the enumerations of the loaded model, and
// the text of the values it prints.
namespace verify {
	namespace gfx_ia = pyxel::gfx::interactions;

	std::string_view name_of(gfx_ia::easing easing) noexcept;
	std::string_view name_of(gfx_ia::slot_type type) noexcept;
	std::string_view name_of(gfx_ia::direction direction) noexcept;
	std::string_view name_of(gfx_ia::object_kind kind) noexcept;
	std::string_view name_of(pyxel::gfx::light_type type) noexcept;

	// "200ms" below a second, "1.5s" from a second up
	std::string format_seconds(double seconds);
	// "200ms ease-in 0ms": duration, easing and delay, as in a transition
	std::string format(gfx_ia::timing const& timing);
	// the value alone, lengths in centimeters and angles in degrees
	std::string format(gfx_ia::slot_value const& value);
	// "rotate.y: 80°"
	std::string format(gfx_ia::group_transformation const& change);
}  // namespace verify
