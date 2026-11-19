// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/solid.hpp>
#include <pyxel/math/glm.hpp>
#include <string>
#include <vector>

namespace pyxel::gfx {
	// Collects the geometry of a model as a tree of nodes. Every node has
	// a name, a local transformation (relative to its parent node), and the
	// meshes (one per material) and lights added while it was the current
	// node.
	//
	// The tree starts with an unnamed root. A named scope (see `scope`) opens
	// a child node of the current one, with the matrix given; the solids
	// added inside are baked with the matrix given to `add_solid`, which is
	// relative to that node. A group passes its matrix to the scope of its
	// name and starts its children from identity, so the transformations
	// of the named groups end up in the nodes, and the ones of the unnamed
	// groups are baked into the vertices.
	//
	// The local matrices of the nodes must be made of a translation,
	// a rotation and a scale only: no shear, as most file formats (glTF,
	// FBX) cannot store it. The scale may be negative; a node with
	// a negative determinant mirrors its contents, and every consumer of
	// the tree must observe that (glTF readers flip the winding for such
	// nodes; the baked vertices have their winding flipped already).
	struct sink {
		using node_id = std::size_t;
		static constexpr node_id root = 0;

		struct glm_solid {
			using face_t = std::vector<size_t>;
			using faces_t = std::vector<face_t>;

			std::vector<glm::dvec3> const& vertices() const noexcept { return vertices_; }
			faces_t const& faces() const noexcept { return faces_; }
			faces_t& faces() noexcept { return faces_; }

			void add_solid(solid const& obj, glm::dmat4 const& matrix);

		private:
			struct dvec3_less {
				bool operator()(glm::dvec3 const& lhs, glm::dvec3 const& rhs) const noexcept {
					if (lhs.x != rhs.x) return lhs.x < rhs.x;
					if (lhs.y != rhs.y) return lhs.y < rhs.y;
					return lhs.z < rhs.z;
				}
			};

			size_t add_vertex(glm::dvec3 const& vertex);

			std::vector<glm::dvec3> vertices_{};
			std::map<glm::dvec3, size_t, dvec3_less> lookup_{};
			faces_t faces_{};
		};

		// a light, with the matrix placing it in its node; the light shines
		// along the -z of that matrix (see light)
		struct placed_light {
			gfx::light light{};
			glm::dmat4 matrix{1.0};
		};

		struct node {
			// empty for the root only
			std::optional<node_id> parent{};
			std::string name{};
			// relative to the parent node
			glm::dmat4 local{1.0};
			// by material; vertices in the space of this node
			std::map<std::string, glm_solid> meshes{};
			// in the order of adding
			std::vector<placed_light> lights{};
			// in the order of opening
			std::vector<node_id> children{};
		};

		sink();

		// bakes the solid with `matrix` (relative to the current node) into
		// the mesh of the current material, in the current node; throws
		// std::invalid_argument, leaving the sink as it was, if the matrix,
		// or a baked vertex, is not finite
		void add_solid(solid const& solid, glm::dmat4 const& matrix);

		// keeps a copy of the light, with `matrix` (relative to the current
		// node), in the current node; the material is not used. A light has
		// no geometry to bake the matrix into, so the matrix is kept as it
		// is, and the same rules apply to it as to the local matrices of the
		// nodes: no shear
		void add_light(light const& light, glm::dmat4 const& matrix);

		// opens a child of the current node and makes it current; nodes are
		// never merged, so two siblings with the same name are two nodes
		void push_node(std::string name, glm::dmat4 const& local);
		// makes the parent of the current node current; does nothing at
		// the root
		void pop_node();

		void push_material(std::string const& name);
		void pop_material();

		// Opens a node for a non-empty name, with `matrix` (identity, if
		// missing), and pushes a non-empty material, for the lifetime of
		// the guard, so they are popped even if a child throws. A matrix
		// without a name has no node to go to, and throws
		// std::invalid_argument.
		class scope_context {
		public:
			scope_context(sink& out,
			              std::string const& name,
			              std::string const& material,
			              std::optional<glm::dmat4> const& matrix = std::nullopt);
			~scope_context();

			scope_context(scope_context const&) = delete;
			scope_context& operator=(scope_context const&) = delete;

		private:
			sink& out_;
			bool has_name_;
			bool has_material_;
		};

		[[nodiscard]] scope_context scope(std::string const& name = {},
		                                  std::string const& material = {},
		                                  std::optional<glm::dmat4> const& matrix = std::nullopt) {
			return {*this, name, material, matrix};
		}

		std::vector<node> const& nodes() const noexcept { return nodes_; }
		node const& at(node_id id) const { return nodes_.at(id); }
		node_id current() const noexcept { return current_; }

		// from the space of the node to the space of the root: the local
		// matrices of the node and of all its ancestors
		glm::dmat4 global_matrix(node_id id) const;

	private:
		std::vector<node> nodes_{};
		node_id current_{root};
		std::vector<std::string> materials_{};
	};
}  // namespace pyxel::gfx
