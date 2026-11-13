// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <optional>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/math/glm.hpp>
#include <span>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// in metres; the same as the plane tolerance of the operations
		constexpr double tolerance = 1e-7;
		// for unit normals; ~0.01 degree
		constexpr double normal_tolerance = 1e-4;

		using face_t = std::vector<size_t>;
		using faces_t = std::vector<face_t>;

		struct plane {
			glm::dvec3 normal{};
			double w{};
		};

		// Newell's method; not normalized, so the length is twice the area
		glm::dvec3 newell(std::span<glm::dvec3 const> points, face_t const& face) {
			glm::dvec3 normal{};
			for (size_t index = 0; index < face.size(); ++index) {
				auto const& cur = points[face[index]];
				auto const& next = points[face[(index + 1) % face.size()]];
				normal.x += (cur.y - next.y) * (cur.z + next.z);
				normal.y += (cur.z - next.z) * (cur.x + next.x);
				normal.z += (cur.x - next.x) * (cur.y + next.y);
			}
			return normal;
		}

		std::optional<plane> plane_of(std::span<glm::dvec3 const> points, face_t const& face) {
			auto const normal = newell(points, face);
			auto const length = glm::length(normal);
			if (!(length > tolerance * tolerance)) return std::nullopt;
			return plane{normal / length, glm::dot(normal / length, points[face[0]])};
		}

		// drops repeated neighbours, including the last and the first
		void drop_repeats(face_t& face) {
			face.erase(std::unique(face.begin(), face.end()), face.end());
			while (face.size() > 1 && face.front() == face.back())
				face.pop_back();
		}

		// ---- 1. welding ----------------------------------------------------

		struct mesh {
			std::vector<glm::dvec3> points{};
			faces_t faces{};
		};

		mesh weld(solid const& obj) {
			auto const& vertices = obj.vertices();

			std::vector<size_t> order(vertices.size());
			std::iota(order.begin(), order.end(), size_t{0});
			std::ranges::sort(order, [&](size_t lhs, size_t rhs) { return vertices[lhs].x < vertices[rhs].x; });

			// sweep along x; each vertex goes to the first close one found
			constexpr auto unassigned = static_cast<size_t>(-1);
			std::vector<size_t> remap(vertices.size(), unassigned);
			mesh result{};
			for (size_t pos = 0; pos < order.size(); ++pos) {
				auto const index = order[pos];
				if (remap[index] != unassigned) continue;

				auto const point = vertices[index].to_glm();
				remap[index] = result.points.size();
				for (size_t next = pos + 1; next < order.size(); ++next) {
					auto const other = vertices[order[next]].to_glm();
					if (other.x - point.x > tolerance) break;
					if (remap[order[next]] == unassigned && glm::distance(point, other) <= tolerance)
						remap[order[next]] = result.points.size();
				}
				result.points.push_back(point);
			}

			for (auto const& face : obj.faces()) {
				if (std::ranges::any_of(face, [&](size_t index) { return index >= vertices.size(); })) {
					continue;
				}
				face_t mapped{};
				mapped.reserve(face.size());
				for (auto const index : face)
					mapped.push_back(remap[index]);
				drop_repeats(mapped);
				if (mapped.size() >= 3 && plane_of(result.points, mapped)) result.faces.push_back(std::move(mapped));
			}
			return result;
		}

		// ---- 2. T-junctions ------------------------------------------------

		void split_edges(mesh& data) {
			std::vector<size_t> by_x(data.points.size());
			std::iota(by_x.begin(), by_x.end(), size_t{0});
			std::ranges::sort(by_x, [&](size_t lhs, size_t rhs) { return data.points[lhs].x < data.points[rhs].x; });

			for (auto& face : data.faces) {
				face_t result{};
				for (size_t index = 0; index < face.size(); ++index) {
					auto const from = face[index];
					auto const to = face[(index + 1) % face.size()];
					result.push_back(from);

					auto const& start = data.points[from];
					auto const& end = data.points[to];
					auto const edge = end - start;
					auto const length = glm::length(edge);
					if (!(length > tolerance)) continue;

					// only the vertices within the x range of the edge
					auto const low = std::min(start.x, end.x) - tolerance;
					auto const high = std::max(start.x, end.x) + tolerance;
					auto it = std::ranges::lower_bound(by_x, low, {}, [&](size_t i) { return data.points[i].x; });

					std::vector<std::pair<double, size_t>> inside{};
					for (; it != by_x.end() && data.points[*it].x <= high; ++it) {
						auto const candidate = *it;
						if (candidate == from || candidate == to) continue;
						auto const along = glm::dot(data.points[candidate] - start, edge) / length;
						if (along <= tolerance || along >= length - tolerance) continue;
						auto const closest = start + edge * (along / length);
						if (glm::distance(closest, data.points[candidate]) <= tolerance)
							inside.emplace_back(along, candidate);
					}
					std::ranges::sort(inside);
					for (auto const& [_, candidate] : inside)
						result.push_back(candidate);
				}
				drop_repeats(result);
				face = std::move(result);
			}
		}

		// ---- 3. coplanar faces ---------------------------------------------

		// boundary loops of the faces, or nothing, if the boundary does not
		// make simple closed loops around the faces
		std::optional<faces_t> merge(std::span<glm::dvec3 const> points, plane const& surface, faces_t const& faces) {
			// an edge shared by two faces is walked once each way, so they cancel out
			std::map<std::pair<size_t, size_t>, int> edges{};
			for (auto const& face : faces) {
				for (size_t index = 0; index < face.size(); ++index) {
					auto const from = face[index];
					auto const to = face[(index + 1) % face.size()];
					auto const reverse = edges.find({to, from});
					if (reverse != edges.end() && reverse->second > 0) {
						if (--reverse->second == 0) edges.erase(reverse);
					} else {
						++edges[{from, to}];
					}
				}
			}

			std::multimap<size_t, size_t> outgoing{};
			for (auto const& [edge, count] : edges) {
				for (int copy = 0; copy < count; ++copy)
					outgoing.emplace(edge.first, edge.second);
			}

			faces_t loops{};
			while (!outgoing.empty()) {
				auto const start = outgoing.begin()->first;
				face_t loop{};
				auto current = start;
				do {
					auto const it = outgoing.find(current);
					if (it == outgoing.end()) return std::nullopt;  // open chain
					loop.push_back(current);
					current = it->second;
					outgoing.erase(it);
				} while (current != start);

				// a loop running the other way round is a hole
				if (glm::dot(newell(points, loop), surface.normal) <= 0) return std::nullopt;
				loops.push_back(std::move(loop));
			}
			return loops;
		}

		void merge_coplanar(mesh& data) {
			struct group {
				plane surface;
				faces_t faces{};
			};

			std::vector<group> groups{};
			for (auto& face : data.faces) {
				auto const surface = *plane_of(data.points, face);
				auto const same = std::ranges::find_if(groups, [&](group const& known) {
					return glm::distance(known.surface.normal, surface.normal) <= normal_tolerance &&
					       std::abs(known.surface.w - surface.w) <= tolerance;
				});
				if (same == groups.end())
					groups.push_back({surface, {std::move(face)}});
				else
					same->faces.push_back(std::move(face));
			}

			data.faces.clear();
			for (auto& [surface, faces] : groups) {
				auto merged = faces.size() > 1 ? merge(data.points, surface, faces) : std::nullopt;
				auto& kept = merged ? *merged : faces;
				data.faces.insert(data.faces.end(), std::make_move_iterator(kept.begin()),
				                  std::make_move_iterator(kept.end()));
			}
		}

		// ---- 4. vertices on straight edges ---------------------------------

		bool is_straight(std::span<glm::dvec3 const> points, size_t prev, size_t cur, size_t next) {
			auto const& start = points[prev];
			auto const edge = points[next] - start;
			auto const length = glm::length(edge);
			if (!(length > tolerance)) return false;
			auto const along = glm::dot(points[cur] - start, edge) / length;
			if (along <= 0 || along >= length) return false;
			return glm::length(glm::cross(points[cur] - start, edge)) / length <= tolerance;
		}

		void drop_straight(mesh& data) {
			// a vertex stays, if it is a corner of at least one face
			std::vector<bool> corner(data.points.size(), false);
			for (auto const& face : data.faces) {
				for (size_t index = 0; index < face.size(); ++index) {
					auto const prev = face[(index + face.size() - 1) % face.size()];
					auto const next = face[(index + 1) % face.size()];
					if (!is_straight(data.points, prev, face[index], next)) corner[face[index]] = true;
				}
			}

			faces_t kept{};
			for (auto& face : data.faces) {
				std::erase_if(face, [&](size_t index) { return !corner[index]; });
				if (face.size() >= 3) kept.push_back(std::move(face));
			}
			data.faces = std::move(kept);
		}

		// only the vertices used by the faces, in the order of first use
		solid to_solid(mesh const& data) {
			solid result{};
			for (auto const& face : data.faces) {
				face_t mapped{};
				mapped.reserve(face.size());
				for (auto const index : face)
					mapped.push_back(result.add_vert(offset::from(data.points[index])));
				result.add_face(mapped);
			}
			return result;
		}
	}  // namespace

	solid cleanup(solid const& obj) {
		auto data = weld(obj);
		split_edges(data);
		merge_coplanar(data);
		drop_straight(data);
		return to_solid(data);
	}
}  // namespace pyxel::gfx
