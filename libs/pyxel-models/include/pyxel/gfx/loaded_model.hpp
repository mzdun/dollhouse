// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <optional>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/loaded_object.hpp>
#include <pyxel/math/aabb.hpp>
#include <pyxel/math/glm.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A model as a viewer holds it once it is read from a file: a tree of nodes
// kept in flat lists, which refer to each other by index, with the meshes,
// materials and lights of the model, and its active objects resolved
// against the nodes and lights.
//
// The loaded model is shared by every placement of the model and never
// changes; a placement keeps what changes in lists of its own, indexed as
// the lists here are (see One storage per placement in
// docs/model-interactions.md).
//
// Every length is in meters and every angle in radians: glTF 2 is in meters
// by definition, and an importer of another format converts it on loading.
// The data is kept in floats, as the files and the GPUs keep it.
namespace pyxel::gfx {
	struct loaded_model {
		struct node {
			// empty for the root only; a parent comes before its children
			std::optional<size_t> parent{};
			std::string name{};
			// relative to the parent node
			glm::mat4 local{1.0f};
			// indices into `meshes` and `lights`
			std::vector<size_t> meshes{};
			std::vector<size_t> lights{};

			bool operator==(node const&) const = default;
		};

		// triangles, with a normal for every vertex
		struct mesh {
			std::vector<glm::vec3> positions{};
			std::vector<glm::vec3> normals{};
			std::vector<unsigned> indices{};  // three per triangle
			size_t material{};                // into `materials`

			bool operator==(mesh const&) const = default;
		};

		// metallic/roughness, as in glTF 2
		struct material {
			std::string name{};
			// linear RGB, with the opacity as alpha
			glm::vec4 base_color{1.0f};
			float metallic{};
			float roughness{1.0f};

			bool operator==(material const&) const = default;
		};

		// A light shines along -z of its node (see gfx::light).
		struct light {
			// the name of its node, which is the name of the light
			std::string name{};
			size_t node{};  // into `nodes`
			light_type type{light_type::point};
			// linear RGB, with the intensity folded in, as Assimp gives it
			glm::vec3 color{1.0f};
			std::optional<float> range{};
			// spot lights only; from the light's direction to the edge
			float inner_cone{};
			float outer_cone{};
			// area lights only; the height is the width for squares and disks
			area_shape shape{area_shape::square};
			float width{};
			float height{};
			float spread{};

			bool operator==(light const&) const = default;
		};

		// An active object, with the nodes and the light it works on.
		struct active_object {
			interactions::loaded_object object{};
			// one per object.groups: the node of the group, empty if the
			// model has no node of that name
			std::vector<std::optional<size_t>> nodes{};
			// the light a light source describes, into `lights`
			std::optional<size_t> light{};

			bool operator==(active_object const&) const = default;
		};

		// in depth-first order, each parent before its children; nodes[0]
		// is the root
		std::vector<node> nodes{};
		std::vector<mesh> meshes{};
		std::vector<material> materials{};
		std::vector<light> lights{};
		std::vector<active_object> objects{};

		bool operator==(loaded_model const&) const = default;

		// The first node of the name, in the order of `nodes`.
		std::optional<size_t> find_node(std::string_view name) const noexcept;

		// The matrix of every node in the space of the model. With `moves`,
		// one per node, the local matrix of each node is followed by its
		// move: `local' = local · M` (see Model space in the specification),
		// for drawing a placement; with no moves, every M is the identity.
		std::vector<glm::mat4> world_matrices(std::span<glm::mat4 const> moves = {}) const;

		// The box around the vertices of every mesh, in the space of the
		// model, with the nodes placed by `world_matrices(moves)`.
		AABB bounds(std::span<glm::mat4 const> moves = {}) const;

		// The move of every node, with each active object settled in its
		// state from `states`, one per object, as indices into its states:
		// the values of the channels of each group the state sets,
		// composed with move_of(). The moves of several objects moving the
		// same node are composed in the order the objects are declared,
		// `M = M1 · M2 · ...`. An object with an empty state, past the end
		// of `states`, or with a state it does not have, moves nothing; so
		// do the light sources.
		std::vector<glm::dmat4> moves(std::span<std::optional<size_t> const> states) const;

		// The states every object starts in, for moves().
		std::vector<std::optional<size_t>> start_states() const;

		// Fills `objects` from the loaded objects, finding their groups among
		// the nodes and their lights among the lights, by name.
		void resolve(interactions::loaded_objects loaded);
	};

	// The move of a group with the values of its channels, composed in the
	// order given: `M = M1 · M2 · ...`, so each one moves the axes already
	// moved by the ones before it (see Channels and Model space in
	// docs/model-interactions.md). In doubles, for the callers writing the
	// result back to a file.
	glm::dmat4 move_of(std::span<interactions::group_transformation const> values);
}  // namespace pyxel::gfx
