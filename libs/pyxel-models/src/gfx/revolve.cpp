// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <cmath>
#include <numbers>
#include <pyxel/gfx/solid.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// twice the area, positive for a counter-clockwise profile, in m²
		double signed_area(std::span<solid::profile_point const> profile) {
			double result = 0.0;
			for (size_t index = 0; index < profile.size(); ++index) {
				auto const& [x0, y0] = profile[index];
				auto const& [x1, y1] = profile[(index + 1) % profile.size()];
				result += x0.count() * y1.count() - x1.count() * y0.count();
			}
			return result;
		}

		// below 1 um², the profile is taken as flat
		constexpr double min_area = 1e-12;
	}  // namespace

	solid solid::revolve(std::span<profile_point const> profile, size_t sides) {
		if (sides < 3) throw std::invalid_argument("revolve: at least 3 sides are needed");
		for (auto const& [x, y] : profile) {
			if (!std::isfinite(x.count()) || !std::isfinite(y.count()))
				throw std::invalid_argument("revolve: the profile points must be finite");
			if (x < length{}) throw std::invalid_argument("revolve: the profile must not cross the Y axis");
		}
		auto const area = signed_area(profile);
		if (!(std::abs(area) > 2 * min_area)) throw std::invalid_argument("revolve: the profile encloses no area");
		auto const counter_clockwise = area > 0;

		// the rings of points, one per profile point; points on the axis
		// collapse to a single point
		std::vector<std::vector<offset>> rings{};
		rings.reserve(profile.size());
		for (auto const& [x, y] : profile) {
			std::vector<offset> ring{};
			ring.reserve(sides);
			for (size_t side = 0; side < sides; ++side) {
				auto const angle = 2 * std::numbers::pi * static_cast<double>(side) / static_cast<double>(sides);
				ring.push_back({length{x * std::cos(angle)}, y, length{x * std::sin(angle)}});
			}
			rings.push_back(std::move(ring));
		}

		// vertices are added, when a face needs them, so the centre of a cap
		// is not left behind unused
		solid result{};
		auto const add_face = [&result](std::vector<offset> const& points) {
			std::vector<size_t> face{};
			face.reserve(points.size());
			for (auto const& point : points)
				face.push_back(result.add_vert(point));
			// a point on the axis turns a quad into a triangle
			face.erase(std::unique(face.begin(), face.end()), face.end());
			if (face.size() > 1 && face.front() == face.back()) face.pop_back();
			result.add_face(face);
		};

		for (size_t index = 0; index < profile.size(); ++index) {
			auto const next = (index + 1) % profile.size();
			auto const& [from_x, from_y] = profile[index];
			auto const& [to_x, to_y] = profile[next];

			auto const from_axis = from_x == length{};
			auto const to_axis = to_x == length{};
			if (from_axis && to_axis) continue;              // along the axis
			if (from_x == to_x && from_y == to_y) continue;  // repeated point

			auto const& from = rings[index];
			auto const& to = rings[next];

			// flat from or to the axis: one cap, instead of a fan of triangles
			if ((from_axis || to_axis) && from_y == to_y) {
				auto cap = from_axis ? to : from;
				// the order the fan of triangles would have
				if (from_axis != counter_clockwise) std::ranges::reverse(cap);
				add_face(cap);
				continue;
			}

			for (size_t side = 0; side < sides; ++side) {
				auto const following = (side + 1) % sides;
				std::vector<offset> face{from[side], to[side], to[following], from[following]};
				if (!counter_clockwise) std::ranges::reverse(face);
				add_face(face);
			}
		}
		return result;
	}
}  // namespace pyxel::gfx
