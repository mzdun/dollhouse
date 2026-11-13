// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

// Binary space partitioning, after Evan Wallace's csg.js: each solid is
// turned into a BSP tree of its faces, the trees clip each other's polygons,
// and the kept polygons make up the result.

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/math/glm.hpp>
#include <span>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// in metres; points closer to a plane than that lie on it
		constexpr double plane_tolerance = 1e-7;

		struct plane {
			glm::dvec3 normal{};
			double w{};

			// Newell's method; counter-clockwise polygons face the viewer
			static std::optional<plane> from(std::span<glm::dvec3 const> vertices) {
				glm::dvec3 normal{};
				glm::dvec3 centroid{};
				for (size_t index = 0; index < vertices.size(); ++index) {
					auto const& cur = vertices[index];
					auto const& next = vertices[(index + 1) % vertices.size()];
					normal.x += (cur.y - next.y) * (cur.z + next.z);
					normal.y += (cur.z - next.z) * (cur.x + next.x);
					normal.z += (cur.x - next.x) * (cur.y + next.y);
					centroid += cur;
				}

				auto const length = glm::length(normal);
				if (!(length > 0.0)) return std::nullopt;
				normal /= length;
				centroid /= static_cast<double>(vertices.size());
				return plane{normal, glm::dot(normal, centroid)};
			}

			void flip() noexcept {
				normal = -normal;
				w = -w;
			}
		};

		struct polygon {
			std::vector<glm::dvec3> vertices{};
			plane surface{};

			void flip() {
				std::ranges::reverse(vertices);
				surface.flip();
			}
		};

		using polygons = std::vector<polygon>;

		enum side : unsigned { coplanar = 0, front = 1, back = 2, spanning = front | back };

		// Puts the polygon, or its pieces, into the matching list; the lists
		// for the coplanar polygons may be the same as front or back.
		void split(plane const& by,
		           polygon const& poly,
		           polygons& coplanar_front,
		           polygons& coplanar_back,
		           polygons& in_front,
		           polygons& in_back) {
			unsigned poly_side = coplanar;
			std::vector<side> sides{};
			sides.reserve(poly.vertices.size());
			for (auto const& vertex : poly.vertices) {
				auto const distance = glm::dot(by.normal, vertex) - by.w;
				auto const vertex_side = distance < -plane_tolerance  ? back
				                         : distance > plane_tolerance ? front
				                                                      : coplanar;
				poly_side |= vertex_side;
				sides.push_back(vertex_side);
			}

			switch (poly_side) {
				case coplanar:
					(glm::dot(by.normal, poly.surface.normal) > 0 ? coplanar_front : coplanar_back).push_back(poly);
					break;
				case front:
					in_front.push_back(poly);
					break;
				case back:
					in_back.push_back(poly);
					break;
				default: {
					std::vector<glm::dvec3> front_part{};
					std::vector<glm::dvec3> back_part{};
					auto const count = poly.vertices.size();
					for (size_t index = 0; index < count; ++index) {
						auto const next = (index + 1) % count;
						auto const side_i = sides[index];
						auto const side_j = sides[next];
						auto const& vertex_i = poly.vertices[index];
						auto const& vertex_j = poly.vertices[next];

						if (side_i != back) front_part.push_back(vertex_i);
						if (side_i != front) back_part.push_back(vertex_i);
						if ((side_i | side_j) == spanning) {
							auto const t =
							    (by.w - glm::dot(by.normal, vertex_i)) / glm::dot(by.normal, vertex_j - vertex_i);
							auto const cut = glm::mix(vertex_i, vertex_j, t);
							front_part.push_back(cut);
							back_part.push_back(cut);
						}
					}
					if (front_part.size() >= 3) in_front.push_back({std::move(front_part), poly.surface});
					if (back_part.size() >= 3) in_back.push_back({std::move(back_part), poly.surface});
					break;
				}
			}
		}

		class node {
		public:
			explicit node(polygons list = {}) { build(std::move(list)); }

			// swaps inside and outside
			void invert() {
				for (auto& poly : polygons_)
					poly.flip();
				if (divider_) divider_->flip();
				if (front_) front_->invert();
				if (back_) back_->invert();
				std::swap(front_, back_);
			}

			// removes the parts of the polygons, which are inside this tree
			polygons clip_polygons(polygons list) const {
				if (!divider_) return list;

				polygons in_front{};
				polygons in_back{};
				for (auto const& poly : list)
					split(*divider_, poly, in_front, in_back, in_front, in_back);

				if (front_) in_front = front_->clip_polygons(std::move(in_front));
				if (back_)
					in_back = back_->clip_polygons(std::move(in_back));
				else
					in_back.clear();

				in_front.insert(in_front.end(), std::make_move_iterator(in_back.begin()),
				                std::make_move_iterator(in_back.end()));
				return in_front;
			}

			// removes the parts of this tree's polygons, which are inside the other tree
			void clip_to(node const& other) {
				polygons_ = other.clip_polygons(std::move(polygons_));
				if (front_) front_->clip_to(other);
				if (back_) back_->clip_to(other);
			}

			polygons all_polygons() const {
				polygons result{};
				collect(result);
				return result;
			}

			void build(polygons list) {
				if (list.empty()) return;
				if (!divider_) divider_ = list.front().surface;

				polygons in_front{};
				polygons in_back{};
				for (auto const& poly : list)
					split(*divider_, poly, polygons_, polygons_, in_front, in_back);

				if (!in_front.empty()) {
					if (!front_) front_ = std::make_unique<node>();
					front_->build(std::move(in_front));
				}
				if (!in_back.empty()) {
					if (!back_) back_ = std::make_unique<node>();
					back_->build(std::move(in_back));
				}
			}

		private:
			void collect(polygons& result) const {
				result.insert(result.end(), polygons_.begin(), polygons_.end());
				if (front_) front_->collect(result);
				if (back_) back_->collect(result);
			}

			std::optional<plane> divider_{};
			std::unique_ptr<node> front_{};
			std::unique_ptr<node> back_{};
			polygons polygons_{};
		};

		polygons to_polygons(solid const& obj) {
			auto const& vertices = obj.vertices();

			polygons result{};
			result.reserve(obj.faces().size());
			for (auto const& face : obj.faces()) {
				if (face.size() < 3) continue;
				if (std::ranges::any_of(face, [&](size_t index) { return index >= vertices.size(); })) {
					continue;
				}

				std::vector<glm::dvec3> points{};
				points.reserve(face.size());
				for (auto const index : face)
					points.push_back(vertices[index].to_glm());

				if (auto const surface = plane::from(points)) result.push_back({std::move(points), *surface});
			}
			return result;
		}

		solid to_solid(polygons const& list) {
			solid result{};
			for (auto const& poly : list) {
				std::vector<size_t> face{};
				face.reserve(poly.vertices.size());
				for (auto const& vertex : poly.vertices) {
					auto const index = result.add_vert(offset::from(vertex));
					// neighbours may collapse into one vertex
					if (face.empty() || face.back() != index) face.push_back(index);
				}
				if (face.size() > 1 && face.front() == face.back()) face.pop_back();
				if (face.size() >= 3) result.add_face(face);
			}
			return result;
		}

		// An empty tree has no plane to clip with, so it would keep every
		// polygon, even inverted, when it should stand for all of the space;
		// hence empty operands are handled before any tree is built.

		polygons unite(polygons lhs, polygons rhs) {
			if (lhs.empty()) return rhs;
			if (rhs.empty()) return lhs;

			node a{std::move(lhs)};
			node b{std::move(rhs)};
			a.clip_to(b);
			b.clip_to(a);
			b.invert();
			b.clip_to(a);
			b.invert();
			a.build(b.all_polygons());
			return a.all_polygons();
		}

		polygons subtract(polygons lhs, polygons rhs) {
			if (lhs.empty() || rhs.empty()) return lhs;

			node a{std::move(lhs)};
			node b{std::move(rhs)};
			a.invert();
			a.clip_to(b);
			b.clip_to(a);
			b.invert();
			b.clip_to(a);
			b.invert();
			a.build(b.all_polygons());
			a.invert();
			return a.all_polygons();
		}

		polygons intersect(polygons lhs, polygons rhs) {
			if (lhs.empty() || rhs.empty()) return {};

			node a{std::move(lhs)};
			node b{std::move(rhs)};
			a.invert();
			b.clip_to(a);
			b.invert();
			a.clip_to(b);
			b.clip_to(a);
			a.build(b.all_polygons());
			a.invert();
			return a.all_polygons();
		}
	}  // namespace

	solid unite(solid const& lhs, solid const& rhs) { return to_solid(unite(to_polygons(lhs), to_polygons(rhs))); }

	solid intersect(solid const& lhs, solid const& rhs) {
		return to_solid(intersect(to_polygons(lhs), to_polygons(rhs)));
	}

	solid subtract(solid const& lhs, solid const& rhs) {
		return to_solid(subtract(to_polygons(lhs), to_polygons(rhs)));
	}

	solid symmetric_difference(solid const& lhs, solid const& rhs) {
		auto const lhs_polygons = to_polygons(lhs);
		auto const rhs_polygons = to_polygons(rhs);
		// (A - B) | (B - A); the two parts only touch, never overlap
		return to_solid(unite(subtract(lhs_polygons, rhs_polygons), subtract(rhs_polygons, lhs_polygons)));
	}
}  // namespace pyxel::gfx
