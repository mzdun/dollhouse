// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <pyxel/gfx/light.hpp>

namespace pyxel::gfx {
	float light::cone_t::at(angle off_axis) const noexcept {
		auto const cos_theta = math::cos(off_axis);
		auto const cos_outer = math::cos(outer_angle);

		if (inner_angle >= outer_angle) return cos_theta >= cos_outer ? 1.0f : 0.0f;

		auto const cos_inner = math::cos(inner_angle);
		auto const factor = std::clamp((cos_theta - cos_outer) / (cos_inner - cos_outer), 0.0, 1.0);
		return static_cast<float>(factor * factor);
	}

	math::area<double> light::area_t::surface() const noexcept {
		auto const rect = width * effective_height();
		if (shape == area_shape::disk || shape == area_shape::ellipse) return rect * (std::numbers::pi / 4);
		return rect;
	}

	float light::falloff_at(length distance) const noexcept {
		auto const d = distance.count();
		if (range) {
			auto const ratio = d / range->count();
			auto const window = std::clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);
			if (window == 0.0) return 0.0f;
			if (d == 0.0) return std::numeric_limits<float>::infinity();
			return static_cast<float>(window * window / (d * d));
		}
		if (d == 0.0) return std::numeric_limits<float>::infinity();
		return static_cast<float>(1.0 / (d * d));
	}

	float light::illuminance_at(offset const& point) const noexcept {
		if (type == light_type::directional) return intensity;

		auto const to_point = point.to_glm();
		auto const dist = glm::length(to_point);
		auto const falloff = intensity * falloff_at(length{dist});
		if (type == light_type::point || dist == 0.0) return falloff;

		auto const cos_theta = glm::dot(direction(), to_point / dist);
		auto const off_axis = rad(std::acos(std::clamp(cos_theta, -1.0, 1.0)));
		if (type == light_type::spot) return falloff * cone.at(off_axis);

		// area: within half of the spread from its direction
		return off_axis <= area.spread / 2 ? falloff : 0.0f;
	}
}  // namespace pyxel::gfx
