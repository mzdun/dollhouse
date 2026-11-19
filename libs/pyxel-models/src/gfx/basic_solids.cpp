// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <pyxel/gfx/solid.hpp>
#include <pyxel/gfx/transform.hpp>
#include <stdexcept>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// `what` names the argument, e.g. "cube: the width"; NaN fails both checks
		void require_positive(length value, char const* what) {
			if (!(value > length{})) throw std::invalid_argument(std::format("{} must be larger than zero", what));
			if (!std::isfinite(value.count())) throw std::invalid_argument(std::format("{} must be finite", what));
		}

		void require_not_negative(length value, char const* what) {
			if (!(value >= length{})) throw std::invalid_argument(std::format("{} must not be negative", what));
			if (!std::isfinite(value.count())) throw std::invalid_argument(std::format("{} must be finite", what));
		}

		// adds the face with its points; repeated neighbours are merged, and
		// faces left with less than three vertices are dropped
		void add_points(solid& result, std::vector<offset> const& points) {
			std::vector<size_t> face{};
			face.reserve(points.size());
			for (auto const& point : points)
				face.push_back(result.add_vert(point));
			face.erase(std::unique(face.begin(), face.end()), face.end());
			while (face.size() > 1 && face.front() == face.back())
				face.pop_back();
			if (face.size() >= 3) result.add_face(face);
		}
	}  // namespace

	solid solid::cube(length width, length height, length depth) {
		require_positive(width, "cube: the width");
		require_positive(height, "cube: the height");
		require_positive(depth, "cube: the depth");

		solid result{};
		result.reserve(8, 6);
		size_t v[8]{};
		for (size_t index = 0; index < 8; ++index) {
			v[index] = result.add_vert(index & 4 ? width : length{}, index & 2 ? height : length{},
			                           index & 1 ? depth : length{});
		}
		// counter-clockwise, when seen from outside
		result.add_face(v[0], v[1], v[3], v[2]);
		result.add_face(v[4], v[6], v[7], v[5]);
		result.add_face(v[0], v[4], v[5], v[1]);
		result.add_face(v[2], v[3], v[7], v[6]);
		result.add_face(v[0], v[2], v[6], v[4]);
		result.add_face(v[1], v[5], v[7], v[3]);
		return result;
	}

	solid solid::prism(length height, length radius, size_t sides) {
		require_positive(height, "prism: the height");
		require_positive(radius, "prism: the radius");
		std::array<profile_point, 4> const profile{
		    {{length{}, length{}}, {radius, length{}}, {radius, height}, {length{}, height}}};
		return revolve(profile, sides);
	}

	solid solid::pyramid(length height, length radius, size_t sides) {
		require_positive(height, "pyramid: the height");
		require_positive(radius, "pyramid: the radius");
		std::array<profile_point, 3> const profile{{{length{}, length{}}, {radius, length{}}, {length{}, height}}};
		return revolve(profile, sides);
	}

	solid solid::sphere(length radius, size_t longitudes) {
		require_positive(radius, "sphere: the radius");
		if (longitudes < 4) throw std::invalid_argument("sphere: at least 4 longitudes are needed");

		// a half circle from the south pole to the north one; the poles are
		// put exactly on the axis, where cos(±pi/2) would only be close to it
		auto const latitudes = longitudes / 2;
		std::vector<profile_point> profile{};
		profile.reserve(latitudes + 1);
		profile.push_back({length{}, -radius});
		for (size_t index = 1; index < latitudes; ++index) {
			auto const angle = std::numbers::pi * (static_cast<double>(index) / static_cast<double>(latitudes) - .5);
			profile.push_back({length{radius * std::cos(angle)}, length{radius * std::sin(angle)}});
		}
		profile.push_back({length{}, radius});
		return revolve(profile, longitudes);
	}

	// stretching along the axes keeps the faces flat
	solid solid::oval_sphere(length rx, std::optional<length> ry, std::optional<length> rz, size_t longitudes) {
		auto const radius_y = ry.value_or(rx);
		auto const radius_z = rz.value_or(rx);
		require_positive(rx, "oval_sphere: the radius along x");
		require_positive(radius_y, "oval_sphere: the radius along y");
		require_positive(radius_z, "oval_sphere: the radius along z");

		auto const circular = sphere(rx, longitudes);
		if (radius_y == rx && radius_z == rx) return circular;
		return circular.transform(
		    scale(identity_matrix, 1.0, radius_y.count() / rx.count(), radius_z.count() / rx.count()));
	}

	solid solid::rounded_cube(length width, length height, length depth, length radius, size_t segments) {
		require_positive(width, "rounded_cube: the width");
		require_positive(height, "rounded_cube: the height");
		require_positive(depth, "rounded_cube: the depth");
		require_not_negative(radius, "rounded_cube: the radius");
		if (segments < 1) throw std::invalid_argument("rounded_cube: at least 1 segment is needed");

		// one radius for all the axes keeps the rounding circular
		auto const r = std::min({radius, length{width / 2}, length{height / 2}, length{depth / 2}});
		return oval_rounded_cube(width, height, depth, r, r, r, segments);
	}

	// A UV sphere, stretched to the radii, cut along the equator and along the
	// four meridians at the axes; each eighth is moved out to its corner of
	// the box. The cuts open up into the flat faces and the quarter cylinders
	// along the edges.
	solid solid::oval_rounded_cube(length width,
	                               length height,
	                               length depth,
	                               length rx,
	                               std::optional<length> ry,
	                               std::optional<length> rz,
	                               size_t segments) {
		require_positive(width, "oval_rounded_cube: the width");
		require_positive(height, "oval_rounded_cube: the height");
		require_positive(depth, "oval_rounded_cube: the depth");
		auto const radius_y = ry.value_or(rx);
		auto const radius_z = rz.value_or(rx);
		require_not_negative(rx, "oval_rounded_cube: the radius along x");
		require_not_negative(radius_y, "oval_rounded_cube: the radius along y");
		require_not_negative(radius_z, "oval_rounded_cube: the radius along z");
		if (segments < 1) throw std::invalid_argument("oval_rounded_cube: at least 1 segment is needed");

		// every radius stops at half of its own dimension
		auto const r_x = std::min(rx, length{width / 2});
		auto const r_y = std::min(radius_y, length{height / 2});
		auto const r_z = std::min(radius_z, length{depth / 2});
		if (!(r_x > length{}) && !(r_y > length{}) && !(r_z > length{})) return cube(width, height, depth);

		// centres of the corner ellipsoids, relative to the centre of the box
		auto const inner_x = length{width / 2} - r_x;
		auto const inner_y = length{height / 2} - r_y;
		auto const inner_z = length{depth / 2} - r_z;

		auto const quarter = std::numbers::pi / 2;
		auto const steps = static_cast<double>(segments);

		// rows from the south pole to the north one, the equator twice
		struct row {
			double angle;
			length centre_y;
		};
		std::vector<row> rows{};
		for (int hemisphere = 0; hemisphere < 2; ++hemisphere) {
			for (size_t step = 0; step <= segments; ++step) {
				rows.push_back(
				    {quarter * (hemisphere - 1 + static_cast<double>(step) / steps), hemisphere ? inner_y : -inner_y});
			}
		}

		// columns around the Y axis, from +x towards +z; the meridians at the
		// axes twice
		struct column {
			double angle;
			length centre_x;
			length centre_z;
		};
		std::vector<column> columns{};
		for (int quadrant = 0; quadrant < 4; ++quadrant) {
			auto const centre_x = quadrant == 0 || quadrant == 3 ? inner_x : -inner_x;
			auto const centre_z = quadrant < 2 ? inner_z : -inner_z;
			for (size_t step = 0; step <= segments; ++step)
				columns.push_back({quarter * (quadrant + static_cast<double>(step) / steps), centre_x, centre_z});
		}

		// the minimal corner is at the origin
		offset const centre{length{width / 2}, length{height / 2}, length{depth / 2}};
		auto const point = [&](row const& lat, column const& lon) {
			// exact zeros at the poles and on the axes keep the cut faces flat
			auto const at_pole = std::abs(std::abs(lat.angle) - quarter) < 1e-12;
			auto const cos_lat = at_pole ? 0.0 : std::cos(lat.angle);
			auto const sin_lat = at_pole ? std::copysign(1.0, lat.angle) : std::sin(lat.angle);
			auto const along = [](double angle) {
				auto const turns = angle / (std::numbers::pi / 2);
				auto const nearest = std::round(turns);
				if (std::abs(turns - nearest) < 1e-12) {
					switch (static_cast<int>(nearest) % 4) {
						case 0:
							return std::pair{1.0, 0.0};
						case 1:
							return std::pair{0.0, 1.0};
						case 2:
							return std::pair{-1.0, 0.0};
						default:
							return std::pair{0.0, -1.0};
					}
				}
				return std::pair{std::cos(angle), std::sin(angle)};
			};
			auto const [cos_lon, sin_lon] = along(lon.angle);
			return offset{length{centre.x + lon.centre_x + r_x * (cos_lat * cos_lon)},
			              length{centre.y + lat.centre_y + r_y * sin_lat},
			              length{centre.z + lon.centre_z + r_z * (cos_lat * sin_lon)}};
		};

		solid result{};
		auto const count = columns.size();
		for (size_t lat = 0; lat + 1 < rows.size(); ++lat) {
			for (size_t lon = 0; lon < count; ++lon) {
				auto const next = (lon + 1) % count;
				add_points(result, {point(rows[lat], columns[lon]), point(rows[lat + 1], columns[lon]),
				                    point(rows[lat + 1], columns[next]), point(rows[lat], columns[next])});
			}
		}

		// the flat bottom and top: the poles of the four quadrants
		std::vector<offset> bottom{};
		std::vector<offset> top{};
		for (auto const& lon : columns) {
			bottom.push_back(point(rows.front(), lon));
			top.push_back(point(rows.back(), lon));
		}
		add_points(result, bottom);
		std::ranges::reverse(top);
		add_points(result, top);
		return result;
	}
}  // namespace pyxel::gfx
