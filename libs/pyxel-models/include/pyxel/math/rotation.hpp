// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cmath>
#include <numbers>
#include <pyxel/math/glm.hpp>

namespace pyxel {
	constexpr double deg_to_rad(double val) noexcept { return val / 180.0 * std::numbers::pi; }
	constexpr float deg_to_rad(float val) noexcept { return val / 180.0f * std::numbers::pi_v<float>; }

	template <typename Floating>
	struct rotation_t {
		static constinit Floating const yaw_north;
		static constinit Floating const yaw_south;
		static constinit Floating const yaw_east;
		static constinit Floating const yaw_west;

		using floating = Floating;
		using vec3 = glm::vec<3, Floating>;

		floating yaw{};
		floating pitch{};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
		constexpr static floating as_floating(float v) noexcept { return v; }
		constexpr static floating as_floating(double v) noexcept { return v; }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

		static rotation_t from(vec3 const& looking) {
			auto const normal = glm::normalize(looking);
			return {.yaw = as_floating(atan2(normal.x, normal.z)), .pitch = as_floating(asin(normal.y))};
		}

		rotation_t& clip() {
			static constexpr auto half_pi = std::numbers::pi_v<floating> / 2;
			static constexpr auto double_pi = std::numbers::pi_v<floating> * 2;

			// [0, 2π); a non-finite yaw stays non-finite (NaN)
			yaw = std::fmod(yaw, double_pi);
			if (yaw < 0) yaw += double_pi;
			if (yaw >= double_pi) yaw = 0;  // -ε + 2π may round up to 2π

			if (pitch < -half_pi) pitch = -half_pi;
			if (pitch > half_pi) pitch = half_pi;

			return *this;
		}

		glm::vec3 direction() const { return {cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)}; }

		glm::vec3 direction_clipped() const { return rotation_t{*this}.clip().direction(); }

		glm::vec3 right() { return {-cos(yaw), 0, sin(yaw)}; }

		friend rotation_t operator+(rotation_t const& lhs, rotation_t const& rhs) {
			return {.yaw = lhs.yaw + rhs.yaw, .pitch = lhs.pitch + rhs.pitch};
		}
	};

	template <typename Floating>
	inline constinit Floating const rotation_t<Floating>::yaw_north = deg_to_rad(static_cast<Floating>(180.0));
	template <typename Floating>
	inline constinit Floating const rotation_t<Floating>::yaw_south = deg_to_rad(static_cast<Floating>(0.0));
	template <typename Floating>
	inline constinit Floating const rotation_t<Floating>::yaw_east = deg_to_rad(static_cast<Floating>(90.0));
	template <typename Floating>
	inline constinit Floating const rotation_t<Floating>::yaw_west = deg_to_rad(static_cast<Floating>(-90.0));

	using rotation = rotation_t<float>;
	using rotation_d = rotation_t<double>;
}  // namespace pyxel
