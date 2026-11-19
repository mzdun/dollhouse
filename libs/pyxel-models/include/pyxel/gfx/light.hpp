// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <optional>
#include <pyxel/gfx/materials.hpp>
#include <pyxel/gfx/space.hpp>
#include <pyxel/math/glm.hpp>
#include <string>

// Lights follow the glTF 2.0 KHR_lights_punctual extension: the falloff is
// physical (inverse square of the distance, optionally windowed to zero at
// `range`) and is not configurable; the brightness is given by the
// `intensity`, in photometric units. Area lights are an addition of ours,
// with no counterpart in KHR_lights_punctual.
//
// A light has no position or direction of its own; as in glTF, it sits at
// the origin of its local space and shines along -z. Its placement comes
// only from the matrix of the group it is in.
namespace pyxel::gfx {
	enum class light_type { directional, point, spot, area };
	enum class area_shape { square, rectangle, disk, ellipse };

	struct light {
		// angles are measured from the light's direction (-z) to the cone's
		// edge; KHR_lights_punctual expects 0 <= inner < outer <= 90°
		struct cone_t {
			angle inner_angle{};
			angle outer_angle{deg(45)};

			// 1 inside the inner cone, 0 outside the outer one; in between,
			// the cosine of `off_axis` is interpolated linearly and the
			// result squared, as in KHR_lights_punctual. When the inner
			// angle is not smaller than the outer, the edge is hard at the
			// outer angle
			float at(angle off_axis) const noexcept;

			bool operator==(cone_t const&) const = default;
		};

		// the emitting surface of an area light; it lies in the local x-y
		// plane, centered at the origin and facing -z, with the width along
		// x and the height along y
		struct area_t {
			area_shape shape{area_shape::square};
			// side of a square / rectangle, or diameter of a disk / ellipse
			length width{math::meters_f{1}};
			// used by rectangles and ellipses only
			length height{math::meters_f{1}};
			// full angle of the emission, centered on the light's direction (-z);
			// 180° covers the whole front half-space, smaller angles focus
			// the light
			angle spread{deg(180)};

			// the height, or the width for squares and disks
			length effective_height() const noexcept {
				return shape == area_shape::rectangle || shape == area_shape::ellipse ? height : width;
			}
			// area of the emitting surface
			math::area<double> surface() const noexcept;

			bool operator==(area_t const&) const = default;
		};

		light_type type{light_type::point};
		std::string name{};
		// sRGB, as everywhere else; the alpha is not used
		gfx::color color{rgb(255, 255, 255)};
		// candela (lm/sr) for point, spot and area lights, lux (lm/m²) for
		// directional lights
		float intensity{1.0f};
		// distance, at which the light reaches zero; none is infinite.
		// Ignored by directional lights
		std::optional<length> range{};
		// used by spot lights
		cone_t cone{};
		// used by area lights
		area_t area{};

		// the direction every light shines along, in its local space
		static glm::dvec3 direction() noexcept { return {0.0, 0.0, -1.0}; }

		// the falloff with the distance, as in KHR_lights_punctual: 1 / d²,
		// windowed by clamp(1 - (d / range)⁴, 0, 1)² when there is a range;
		// infinite at d = 0
		float falloff_at(length distance) const noexcept;

		// illuminance at `point`, given in the local space of the light, in
		// lux, on a surface facing the light: the intensity, with the
		// falloff and the spot cone or the area spread, as applicable for
		// the type; the area is treated as a point, emitting only to its
		// front
		float illuminance_at(offset const& point) const noexcept;

		bool operator==(light const&) const = default;
	};
}  // namespace pyxel::gfx
