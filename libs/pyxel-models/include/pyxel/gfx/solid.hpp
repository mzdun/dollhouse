// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <map>
#include <optional>
#include <pyxel/gfx/space.hpp>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	struct solid {
		// (x, y) in the XY plane: x is the distance from the Y axis
		using profile_point = std::pair<length, length>;

		solid() = default;
		// vertices are kept as given, duplicates included, since the faces
		// refer to their indices; throws std::invalid_argument for a vertex
		// with a coordinate, which is not finite
		solid(std::vector<offset> vertices, std::vector<std::vector<size_t>> faces);

		// Solid of revolution: the profile, a closed polygon in the XY plane,
		// is copied `sides` times around the Y axis, and the copies are joined
		// with faces. The last point is joined back to the first one, segments
		// lying on the axis add no faces, and a segment going flat out from
		// the axis becomes a single cap, so
		//
		//   - (0, 0), (r, 0), (r, h), (0, h) is a regular prism,
		//   - (0, 0), (r, 0), (0, h) is a regular pyramid,
		//   - (r1, 0), (r2, 0), (r2, h), (r1, h) is a tube with a hole.
		//
		// The coordinates are kept as given, so a profile starting at (0, 0)
		// has the origin in the middle of its base. Either direction of the
		// profile gives faces pointing outwards; the profile must not cross
		// itself. Throws std::invalid_argument, if `sides` is less than 3, if
		// any x is negative, or if the profile encloses no area.
		static solid revolve(std::span<profile_point const> profile, size_t sides);

		// Basic solids. The boxes have their minimal corner at the origin, the
		// prism and the pyramid stand on the XZ plane, with the origin in the
		// middle of the base, and the sphere has the origin in its centre.
		// Throw std::invalid_argument for lengths not larger than zero, or for
		// too few sides.

		// box of the given width (x), height (y) and depth (z), from (0, 0, 0)
		// to (width, height, depth)
		static solid cube(length width, length height, length depth);

		// regular prism of `sides` sides, around a circle of `radius`
		static solid prism(length height, length radius, size_t sides);

		// regular pyramid of `sides` sides, around a circle of `radius`
		static solid pyramid(length height, length radius, size_t sides);

		// UV sphere, going from y = -radius to y = radius; `longitudes`
		// meridians (at least 4) and half as many bands of latitude
		static solid sphere(length radius, size_t longitudes);

		// sphere stretched along the axes, with the radii rx, ry and rz; the
		// missing ones are equal to rx
		static solid oval_sphere(length rx, std::optional<length> ry, std::optional<length> rz, size_t longitudes);

		// box with its edges and corners rounded with `radius`, but never
		// more than half of the smallest dimension; `segments` per quarter of
		// a circle (at least 1); a zero radius gives a plain box
		static solid rounded_cube(length width, length height, length depth, length radius, size_t segments);

		// rounded cube with the edges and corners rounded with separate radii
		// along each axis, each of them never more than half of its own
		// dimension; the missing radii are equal to rx; all radii zero give
		// a plain box
		static solid oval_rounded_cube(length width,
		                               length height,
		                               length depth,
		                               length rx,
		                               std::optional<length> ry,
		                               std::optional<length> rz,
		                               size_t segments);

		void reserve(size_t vertex_count, size_t face_count);

		std::vector<offset> const& vertices() const noexcept { return vertices_; }

		std::vector<std::vector<size_t>> const& faces() const noexcept { return faces_; }

		// returns index of an equal vertex, if there is one already; only the
		// new vertices are appended; throws std::invalid_argument for NaN or
		// infinite coordinates, which have no place in the lookup map
		size_t add_vert(offset const& v);
		size_t add_vert(length x, length y, length z) { return add_vert({x, y, z}); }

		template <std::same_as<size_t>... Index>
		void add_face(Index... index) {
			faces_.push_back({index...});
		}

		void add_face(std::vector<size_t> const& indexes);

		// true, if every face refers only to the vertices added so far; faces
		// may be added before their vertices, so this can change over time
		bool is_valid() const noexcept;

		// copy with every vertex transformed as a point; if the matrix
		// mirrors, the faces are reversed, so they still face outwards
		solid transform(glm::dmat4 const& matrix) const;

	private:
		struct offset_less {
			bool operator()(offset const& lhs, offset const& rhs) const noexcept {
				return std::tie(lhs.x, lhs.y, lhs.z) < std::tie(rhs.x, rhs.y, rhs.z);
			}
		};

		std::vector<offset> vertices_{};
		std::vector<std::vector<size_t>> faces_{};
		std::map<offset, size_t, offset_less> lookup_{};
	};
}  // namespace pyxel::gfx
