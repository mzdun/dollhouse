// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <cstdint>
#include <format>
#include <numbers>
#include <optional>
#include <pyxel/base/str.hpp>
#include <pyxel/gfx/loaded_model.hpp>
#include <pyxel/io/import.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "gltf2.hpp"

namespace pyxel::io {
	namespace {
		using gfx::loaded_model;

		// Assimp is row-major, glm is column-major (m[column][row])
		glm::mat4 to_glm(aiMatrix4x4 const& m) noexcept {
			return {m.a1, m.b1, m.c1, m.d1,  //
			        m.a2, m.b2, m.c2, m.d2,  //
			        m.a3, m.b3, m.c3, m.d3,  //
			        m.a4, m.b4, m.c4, m.d4};
		}

		glm::vec3 to_glm(aiVector3D const& v) noexcept { return {v.x, v.y, v.z}; }
		glm::vec3 to_glm(aiColor3D const& c) noexcept { return {c.r, c.g, c.b}; }

		template <typename Value>
		std::optional<Value> meta(aiNode const& node, char const* key) {
			Value result{};
			if (!node.mMetaData || !node.mMetaData->Get(key, result)) return std::nullopt;
			return result;
		}

		// the root first, every parent before its children, the children in
		// their order
		void add_nodes(aiNode const& root, loaded_model& model, std::vector<aiNode const*>& sources) {
			struct pending {
				aiNode const* node;
				std::optional<size_t> parent;
			};
			std::vector<pending> stack{{&root, std::nullopt}};
			while (!stack.empty()) {
				auto const [source, parent] = stack.back();
				stack.pop_back();

				auto const index = model.nodes.size();
				auto& node = model.nodes.emplace_back();
				node.parent = parent;
				node.name = source->mName.C_Str();
				node.local = to_glm(source->mTransformation);
				node.meshes.assign(source->mMeshes, source->mMeshes + source->mNumMeshes);
				sources.push_back(source);

				for (auto child = source->mNumChildren; child-- > 0;)
					stack.push_back({source->mChildren[child], index});
			}
		}

		loaded_model::mesh to_mesh(aiMesh const& source) {
			loaded_model::mesh result{.material = source.mMaterialIndex};
			result.positions.reserve(source.mNumVertices);
			for (unsigned index = 0; index < source.mNumVertices; ++index)
				result.positions.push_back(to_glm(source.mVertices[index]));
			if (source.mNormals) {
				result.normals.reserve(source.mNumVertices);
				for (unsigned index = 0; index < source.mNumVertices; ++index)
					result.normals.push_back(to_glm(source.mNormals[index]));
			}
			// points and lines are left out
			for (unsigned index = 0; index < source.mNumFaces; ++index) {
				auto const& face = source.mFaces[index];
				if (face.mNumIndices != 3) continue;
				result.indices.insert(result.indices.end(), face.mIndices, face.mIndices + 3);
			}
			return result;
		}

		loaded_model::material to_material(aiMaterial const& source) {
			loaded_model::material result{};
			if (aiString name{}; source.Get(AI_MATKEY_NAME, name) == aiReturn_SUCCESS) result.name = name.C_Str();

			// the base color of glTF 2 carries the opacity; other formats
			// have a diffuse color and an opacity of their own
			if (aiColor4D base{}; source.Get(AI_MATKEY_BASE_COLOR, base) == aiReturn_SUCCESS) {
				result.base_color = {base.r, base.g, base.b, base.a};
			} else {
				if (aiColor4D diffuse{}; source.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == aiReturn_SUCCESS)
					result.base_color = {diffuse.r, diffuse.g, diffuse.b, 1.0f};
				if (float opacity{}; source.Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS)
					result.base_color.a = opacity;
			}
			source.Get(AI_MATKEY_METALLIC_FACTOR, result.metallic);
			source.Get(AI_MATKEY_ROUGHNESS_FACTOR, result.roughness);
			return result;
		}

		gfx::light_type to_type(aiLightSourceType type) noexcept {
			switch (type) {
				case aiLightSource_DIRECTIONAL:
					return gfx::light_type::directional;
				case aiLightSource_SPOT:
					return gfx::light_type::spot;
				case aiLightSource_AREA:
					return gfx::light_type::area;
				default:
					break;
			}
			return gfx::light_type::point;
		}

		// The light goes to the first node of its name; Assimp finds it the
		// same way. A light with no node of its name is left out, as there is
		// nothing to place it by.
		void add_light(aiLight const& source, loaded_model& model, std::vector<aiNode const*> const& sources) {
			auto const node = model.find_node(source.mName.C_Str());
			if (!node) return;

			auto const& tags = *sources[*node];
			loaded_model::light result{
			    .name = source.mName.C_Str(),
			    .node = *node,
			    .type = to_type(source.mType),
			    .color = to_glm(source.mColorDiffuse),
			    .range = meta<float>(tags, gltf2::range_key),
			};
			if (result.type == gfx::light_type::spot) {
				result.inner_cone = source.mAngleInnerCone;
				result.outer_cone = source.mAngleOuterCone;
			}
			if (result.type == gfx::light_type::area) {
				result.width = source.mSize.x;
				result.height = source.mSize.y;
				// squares and disks come back as rectangles and ellipses
				// with both sides the same
				auto const shape = meta<int32_t>(tags, gltf2::area_shape_key);
				result.shape = shape == gltf2::area_ellipse ? gfx::area_shape::ellipse : gfx::area_shape::rectangle;
				result.spread = meta<float>(tags, gltf2::area_spread_key).value_or(std::numbers::pi_v<float>);
			}

			model.nodes[*node].lights.push_back(model.lights.size());
			model.lights.push_back(std::move(result));
		}
	}  // namespace

	imported_model import_model(std::filesystem::path const& path) {
		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		importer.SetPropertyBool(AI_CONFIG_FBX_CONVERT_TO_M, true);

		// Assimp takes the path as UTF-8 (see export_scene)
		auto const path_u8 = as_str(path.u8string());
		auto const* scene =
		    importer.ReadFile(path_u8, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_ValidateDataStructure);
		if (!scene || !scene->mRootNode) {
			throw std::runtime_error(std::format("cannot read {}: {}", path_u8, importer.GetErrorString()));
		}

		imported_model result{};
		auto& model = result.model;

		std::vector<aiNode const*> sources{};
		add_nodes(*scene->mRootNode, model, sources);

		model.meshes.reserve(scene->mNumMeshes);
		for (unsigned index = 0; index < scene->mNumMeshes; ++index)
			model.meshes.push_back(to_mesh(*scene->mMeshes[index]));
		model.materials.reserve(scene->mNumMaterials);
		for (unsigned index = 0; index < scene->mNumMaterials; ++index)
			model.materials.push_back(to_material(*scene->mMaterials[index]));
		for (unsigned index = 0; index < scene->mNumLights; ++index)
			add_light(*scene->mLights[index], model, sources);

		gfx::interactions::active_objects declared{};
		if (auto const* found = gltf2::interactions_of(importer, scene)) {
			declared = found->objects;
			result.notes = found->notes;
		}
		std::vector<std::string> light_names{};
		light_names.reserve(model.lights.size());
		for (auto const& light : model.lights)
			light_names.push_back(light.name);
		model.resolve(gfx::interactions::load(declared, light_names));

		return result;
	}
}  // namespace pyxel::io
