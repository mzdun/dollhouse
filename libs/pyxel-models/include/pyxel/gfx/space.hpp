// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <pyxel/math/glm.hpp>
#include <pyxel/math/value.hpp>
#include <utility>

namespace pyxel::gfx {
	using length = math::metres_f;
	using angle = math::radians;

	inline constexpr length cm(double value) noexcept { return math::centimeters_f{value}; }
	inline constexpr length mm(double value) noexcept { return math::millimeters_f{value}; }

	inline constexpr angle rad(double value) noexcept { return math::radians{value}; }
	inline constexpr angle deg(double value) noexcept { return math::degrees{value}; }

	enum class axis : unsigned { x, y, z };

	inline constexpr std::array<axis, 3> axes{axis::x, axis::y, axis::z};

	// ---- offset ----------------------------------------------------------

	struct offset {
		length x{};
		length y{};
		length z{};

		// fields to replace; the empty ones are kept
		struct changes {
			std::optional<length> x{};
			std::optional<length> y{};
			std::optional<length> z{};
		};

		static constexpr offset at_x(length x) noexcept { return {.x = x}; }
		static constexpr offset at_y(length y) noexcept { return {.y = y}; }
		static constexpr offset at_z(length z) noexcept { return {.z = z}; }

		// glm vectors are in metres
		static offset from(glm::dvec3 const& v) noexcept { return {length{v.x}, length{v.y}, length{v.z}}; }
		glm::dvec3 to_glm() const noexcept { return {x.count(), y.count(), z.count()}; }

		constexpr offset replace(changes const& diff) const noexcept {
			return {diff.x.value_or(x), diff.y.value_or(y), diff.z.value_or(z)};
		}

		constexpr length& operator[](axis a) noexcept { return this->*member(a); }
		constexpr length const& operator[](axis a) const noexcept { return this->*member(a); }

		// treats the offset as a point (w = 1); the result is divided by
		// the transformed w, so projective matrices are supported as well
		offset apply(glm::dmat4 const&) const noexcept;

		// treats the offset as a direction (w = 0); translation is ignored
		offset apply_direction(glm::dmat4 const&) const noexcept;

		friend constexpr bool operator==(offset const&, offset const&) noexcept = default;

		constexpr offset& operator+=(offset const& rhs) noexcept {
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		constexpr offset& operator-=(offset const& rhs) noexcept {
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;
			return *this;
		}

		template <math::Scalar S>
		constexpr offset& operator*=(S rhs) noexcept {
			x = length{x * rhs};
			y = length{y * rhs};
			z = length{z * rhs};
			return *this;
		}

		template <math::Scalar S>
		constexpr offset& operator/=(S rhs) noexcept {
			x = length{x / rhs};
			y = length{y / rhs};
			z = length{z / rhs};
			return *this;
		}

		friend constexpr offset operator-(offset const& off) noexcept { return {-off.x, -off.y, -off.z}; }

		friend constexpr offset operator+(offset lhs, offset const& rhs) noexcept { return lhs += rhs; }
		friend constexpr offset operator-(offset lhs, offset const& rhs) noexcept { return lhs -= rhs; }

		template <math::Scalar S>
		friend constexpr offset operator*(offset lhs, S rhs) noexcept {
			return lhs *= rhs;
		}

		template <math::Scalar S>
		friend constexpr offset operator*(S lhs, offset rhs) noexcept {
			return rhs *= lhs;
		}

		template <math::Scalar S>
		friend constexpr offset operator/(offset lhs, S rhs) noexcept {
			return lhs /= rhs;
		}

	private:
		static constexpr length offset::* member(axis a) noexcept {
			constexpr std::array<length offset::*, 3> members{&offset::x, &offset::y, &offset::z};
			return members[std::to_underlying(a)];
		}
	};

	constexpr math::area<double> distance_squared(offset const& lhs, offset const& rhs) noexcept {
		auto const diff = rhs - lhs;
		return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
	}

	inline length distance(offset const& lhs, offset const& rhs) noexcept {
		return length{std::sqrt(distance_squared(lhs, rhs).count())};
	}

	// t = 0 gives `from`, t = 1 gives `to`; t outside [0, 1] extrapolates
	template <math::Scalar S>
	constexpr offset lerp(offset const& from, offset const& to, S t) noexcept {
		return from + (to - from) * t;
	}

	constexpr offset midpoint(offset const& lhs, offset const& rhs) noexcept { return (lhs + rhs) / 2; }

	// per-axis minimum / maximum, e.g. for corners of a bounding box
	constexpr offset min_pt(offset const& lhs, offset const& rhs) noexcept {
		return {rhs.x < lhs.x ? rhs.x : lhs.x, rhs.y < lhs.y ? rhs.y : lhs.y, rhs.z < lhs.z ? rhs.z : lhs.z};
	}

	constexpr offset max_pt(offset const& lhs, offset const& rhs) noexcept {
		return {lhs.x < rhs.x ? rhs.x : lhs.x, lhs.y < rhs.y ? rhs.y : lhs.y, lhs.z < rhs.z ? rhs.z : lhs.z};
	}

	// every axis differs by at most `tolerance`
	constexpr bool near(offset const& lhs, offset const& rhs, length tolerance = math::micrometers{1}) noexcept {
		return math::abs(lhs.x - rhs.x) <= tolerance && math::abs(lhs.y - rhs.y) <= tolerance &&
		       math::abs(lhs.z - rhs.z) <= tolerance;
	}

	// ---- size ------------------------------------------------------------

	// width runs along x, height along y and depth along z; `member` below is
	// the only place, where this is spelled out, everything else uses `axis`
	struct size {
		length width{};
		length height{};
		length depth{};

		// fields to replace; the empty ones are kept
		struct changes {
			std::optional<length> width{};
			std::optional<length> height{};
			std::optional<length> depth{};
		};

		// per-axis scale factors; the empty ones are treated as 1
		struct factors {
			std::optional<double> x{};
			std::optional<double> y{};
			std::optional<double> z{};

			constexpr std::optional<double> const& operator[](axis a) const noexcept {
				return a == axis::x ? x : a == axis::y ? y : z;
			}
		};

		// per-axis growth (dx, dy, dz); the empty ones are zero
		struct deltas {
			std::optional<length> x{};
			std::optional<length> y{};
			std::optional<length> z{};

			constexpr std::optional<length> const& operator[](axis a) const noexcept {
				return a == axis::x ? x : a == axis::y ? y : z;
			}
		};

		static constexpr size cube(length edge) noexcept { return {edge, edge, edge}; }

		// box spanned between two corners; negative, if `max` lies below `min`
		static constexpr size from_corners(offset const& min, offset const& max) noexcept {
			size result{};
			for (auto const a : axes)
				result[a] = max[a] - min[a];
			return result;
		}

		constexpr size replace(changes const& diff) const noexcept {
			return {diff.width.value_or(width), diff.height.value_or(height), diff.depth.value_or(depth)};
		}

		constexpr length& operator[](axis a) noexcept { return this->*member(a); }
		constexpr length const& operator[](axis a) const noexcept { return this->*member(a); }

		constexpr size scaled(factors const& by) const noexcept {
			size result{};
			for (auto const a : axes)
				result[a] = length{(*this)[a] * by[a].value_or(1.0)};
			return result;
		}

		// grows every dimension by its delta, once; negative deltas shrink
		constexpr size inflate(deltas const& by) const noexcept {
			size result{};
			for (auto const a : axes)
				result[a] = (*this)[a] + by[a].value_or(length{});
			return result;
		}

		constexpr math::volume<double> volume() const noexcept {
			return (*this)[axis::x] * (*this)[axis::y] * (*this)[axis::z];
		}

		// area covered on the ground (the x-z plane)
		constexpr math::area<double> footprint() const noexcept { return (*this)[axis::x] * (*this)[axis::z]; }

		// middle of a box with its minimal corner at origin
		constexpr offset center() const noexcept { return to_offset() / 2; }

		constexpr offset to_offset() const noexcept {
			offset result{};
			for (auto const a : axes)
				result[a] = (*this)[a];
			return result;
		}

		friend constexpr bool operator==(size const&, size const&) noexcept = default;

		constexpr size& operator+=(size const& rhs) noexcept {
			width += rhs.width;
			height += rhs.height;
			depth += rhs.depth;
			return *this;
		}

		constexpr size& operator-=(size const& rhs) noexcept {
			width -= rhs.width;
			height -= rhs.height;
			depth -= rhs.depth;
			return *this;
		}

		template <math::Scalar S>
		constexpr size& operator*=(S rhs) noexcept {
			width = length{width * rhs};
			height = length{height * rhs};
			depth = length{depth * rhs};
			return *this;
		}

		template <math::Scalar S>
		constexpr size& operator/=(S rhs) noexcept {
			width = length{width / rhs};
			height = length{height / rhs};
			depth = length{depth / rhs};
			return *this;
		}

		friend constexpr size operator+(size lhs, size const& rhs) noexcept { return lhs += rhs; }
		friend constexpr size operator-(size lhs, size const& rhs) noexcept { return lhs -= rhs; }

		template <math::Scalar S>
		friend constexpr size operator*(size lhs, S rhs) noexcept {
			return lhs *= rhs;
		}

		template <math::Scalar S>
		friend constexpr size operator*(S lhs, size rhs) noexcept {
			return rhs *= lhs;
		}

		template <math::Scalar S>
		friend constexpr size operator/(size lhs, S rhs) noexcept {
			return lhs /= rhs;
		}

		// the opposite corner of a box placed at `corner`
		friend constexpr offset operator+(size const& lhs, offset const& corner) noexcept {
			return corner + lhs.to_offset();
		}

		friend constexpr offset operator+(offset const& corner, size const& rhs) noexcept {
			return corner + rhs.to_offset();
		}

	private:
		static constexpr length size::* member(axis a) noexcept {
			constexpr std::array<length size::*, 3> members{&size::width, &size::height, &size::depth};
			return members[std::to_underlying(a)];
		}
	};
}  // namespace pyxel::gfx
