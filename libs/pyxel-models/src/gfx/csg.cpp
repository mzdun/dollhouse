// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

// Binary space partitioning, after Evan Wallace's csg.js: each solid is
// turned into a BSP tree of its faces, the trees clip each other's polygons,
// and the kept polygons make up the result.

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/math/glm.hpp>
#include <span>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	namespace {
		// in meters; points closer to a plane than that lie on it
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

		// The tree is walked with explicit stacks, never recursively: the faces
		// of a convex solid all lie behind each other's planes, so its tree is
		// a chain as deep as the solid has faces, whichever face divides, and
		// a recursive walk would overflow the call stack for dense solids.
		class node {
		public:
			explicit node(polygons list = {}) { build(std::move(list)); }

			node(node const&) = delete;
			node& operator=(node const&) = delete;
			node(node&&) = delete;
			node& operator=(node&&) = delete;

			// the children are taken over first, so destroying one does not recurse
			~node() {
				std::vector<std::unique_ptr<node>> pending{};
				take_children(pending);
				while (!pending.empty()) {
					auto child = std::move(pending.back());
					pending.pop_back();
					child->take_children(pending);
				}
			}

			// swaps inside and outside
			void invert() {
				std::vector<node*> pending{this};
				while (!pending.empty()) {
					auto* current = pending.back();
					pending.pop_back();

					for (auto& poly : current->polygons_)
						poly.flip();
					if (current->divider_) current->divider_->flip();
					std::swap(current->front_, current->back_);
					current->push_children(pending);
				}
			}

			// removes the parts of the polygons, which are inside this tree; the
			// pieces kept in front of a divider come before the ones behind it
			polygons clip_polygons(polygons list) const {
				if (!divider_) return list;

				polygons result{};
				// LIFO: the front half is pushed last, to be clipped first
				std::vector<std::pair<node const*, polygons>> pending{};
				pending.emplace_back(this, std::move(list));
				while (!pending.empty()) {
					auto [current, items] = std::move(pending.back());
					pending.pop_back();

					polygons in_front{};
					polygons in_back{};
					for (auto const& poly : items)
						split(*current->divider_, poly, in_front, in_back, in_front, in_back);

					// with no back, the pieces behind the divider are inside
					if (current->back_ && !in_back.empty())
						pending.emplace_back(current->back_.get(), std::move(in_back));

					if (current->front_) {
						if (!in_front.empty()) pending.emplace_back(current->front_.get(), std::move(in_front));
					} else {
						result.insert(result.end(), std::make_move_iterator(in_front.begin()),
						              std::make_move_iterator(in_front.end()));
					}
				}
				return result;
			}

			// removes the parts of this tree's polygons, which are inside the other tree
			void clip_to(node const& other) {
				std::vector<node*> pending{this};
				while (!pending.empty()) {
					auto* current = pending.back();
					pending.pop_back();

					current->polygons_ = other.clip_polygons(std::move(current->polygons_));
					current->push_children(pending);
				}
			}

			// the polygons of a node come before the ones in front of it, then
			// the ones behind it
			polygons all_polygons() const {
				polygons result{};
				std::vector<node const*> pending{this};
				while (!pending.empty()) {
					auto const* current = pending.back();
					pending.pop_back();

					result.insert(result.end(), current->polygons_.begin(), current->polygons_.end());
					if (current->back_) pending.push_back(current->back_.get());
					if (current->front_) pending.push_back(current->front_.get());
				}
				return result;
			}

			void build(polygons list) {
				std::vector<std::pair<node*, polygons>> pending{};
				pending.emplace_back(this, std::move(list));
				while (!pending.empty()) {
					auto [current, items] = std::move(pending.back());
					pending.pop_back();

					if (items.empty()) continue;
					if (!current->divider_) current->divider_ = items.front().surface;

					polygons in_front{};
					polygons in_back{};
					for (auto const& poly : items)
						split(*current->divider_, poly, current->polygons_, current->polygons_, in_front, in_back);

					if (!in_front.empty()) {
						if (!current->front_) current->front_ = std::make_unique<node>();
						pending.emplace_back(current->front_.get(), std::move(in_front));
					}
					if (!in_back.empty()) {
						if (!current->back_) current->back_ = std::make_unique<node>();
						pending.emplace_back(current->back_.get(), std::move(in_back));
					}
				}
			}

		private:
			template <typename Node>
			void push_children(std::vector<Node*>& pending) const {
				if (front_) pending.push_back(front_.get());
				if (back_) pending.push_back(back_.get());
			}

			void take_children(std::vector<std::unique_ptr<node>>& pending) {
				if (front_) pending.push_back(std::move(front_));
				if (back_) pending.push_back(std::move(back_));
			}

			std::optional<plane> divider_{};
			std::unique_ptr<node> front_{};
			std::unique_ptr<node> back_{};
			polygons polygons_{};
		};

		polygons to_polygons(solid const& obj) {
			auto const& vertices = obj.vertices();

			auto const past_the_end = [&](size_t index) { return index >= vertices.size(); };

			polygons result{};
			result.reserve(obj.faces().size());
			for (auto const& face : obj.faces()) {
				if (face.size() < 3) continue;
				if (std::ranges::any_of(face, past_the_end)) continue;

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
					// neighbors may collapse into one vertex
					if (face.empty() || face.back() != index) face.push_back(index);
				}
				if (face.size() > 1 && face.front() == face.back()) face.pop_back();
				if (face.size() >= 3) result.add_face(face);
			}
			return result;
		}

		struct bounds {
			glm::dvec3 min{std::numeric_limits<double>::infinity()};
			glm::dvec3 max{-std::numeric_limits<double>::infinity()};

			static bounds of(polygons const& list) {
				bounds result{};
				for (auto const& poly : list) {
					for (auto const& vertex : poly.vertices) {
						result.min = glm::min(result.min, vertex);
						result.max = glm::max(result.max, vertex);
					}
				}
				return result;
			}

			// the boxes are apart by more than the plane tolerance on at least
			// one axis; boxes, which only touch, are not apart, so the faces
			// they share still go through the trees
			bool is_apart_from(bounds const& other) const noexcept {
				for (glm::length_t axis = 0; axis < 3; ++axis) {
					if (max[axis] + plane_tolerance < other.min[axis]) return true;
					if (other.max[axis] + plane_tolerance < min[axis]) return true;
				}
				return false;
			}
		};

		// Solids with bounding boxes apart cannot clip each other, so their
		// polygons skip the trees: A | B is both lists, A - B is A, A & B is
		// empty. Besides saving the work, the polygons are not cut along the
		// planes of the other solid for nothing.
		bool are_apart(polygons const& lhs, polygons const& rhs) {
			return bounds::of(lhs).is_apart_from(bounds::of(rhs));
		}

		// An empty tree has no plane to clip with, so it would keep every
		// polygon, even inverted, when it should stand for all of the space;
		// hence empty operands are handled before any tree is built.

		polygons unite(polygons lhs, polygons rhs) {
			if (lhs.empty()) return rhs;
			if (rhs.empty()) return lhs;
			if (are_apart(lhs, rhs)) {
				lhs.insert(lhs.end(), std::make_move_iterator(rhs.begin()), std::make_move_iterator(rhs.end()));
				return lhs;
			}

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
			if (lhs.empty() || rhs.empty() || are_apart(lhs, rhs)) return lhs;

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
			if (lhs.empty() || rhs.empty() || are_apart(lhs, rhs)) return {};

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
