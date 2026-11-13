// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "export.hpp"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <assimp/Exporter.hpp>
#include <cmath>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <variant>

namespace shapes {
	namespace {
		using pyxel::gfx::materials;
		using pyxel::gfx::sink;

		// object path -> node; the meshes are attached to the node of their
		// full path, so the hierarchy of the groups survives the export
		struct node_tree {
			std::vector<unsigned> meshes{};
			std::map<std::string, node_tree> children{};

			node_tree& at(sink::strings const& path) {
				auto* node = this;
				for (auto const& name : path)
					node = &node->children[name];
				return *node;
			}

			aiNode* to_ai(std::string const& name, aiNode* parent) const {
				auto* node = new aiNode{name};
				node->mParent = parent;

				node->mNumMeshes = static_cast<unsigned>(meshes.size());
				if (!meshes.empty()) {
					node->mMeshes = new unsigned[meshes.size()];
					std::copy(meshes.begin(), meshes.end(), node->mMeshes);
				}

				node->mNumChildren = static_cast<unsigned>(children.size());
				if (!children.empty()) {
					node->mChildren = new aiNode*[children.size()];
					auto* dst = node->mChildren;
					for (auto const& [child_name, child] : children)
						*dst++ = child.to_ai(child_name, node);
				}
				return node;
			}
		};

		// registered names follow `Material`, `Material.001`, ..., so this
		// one cannot clash with them
		constexpr auto missing_material_name = "Missing";
		constexpr materials::paint_opts missing_material{.color = pyxel::gfx::rgb(0xFF, 0x00, 0xFF)};

		// Assimp colors are linear, pyxel colors are sRGB
		float to_linear(uint8_t channel) noexcept {
			auto const value = static_cast<float>(channel) / 255.0f;
			if (value <= 0.04045f) return value / 12.92f;
			return std::pow((value + 0.055f) / 1.055f, 2.4f);
		}

		aiMaterial* make_material(std::string const& name, materials::paint_opts const& paint) {
			auto* material = new aiMaterial{};
			aiString const ai_name{name};
			material->AddProperty(&ai_name, AI_MATKEY_NAME);

			auto const opacity = std::clamp(paint.color.alpha, 0.0f, 1.0f);
			material->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);

			// RGBA: when writing KHR_materials_specular, the glTF 2 exporter
			// replaces the base color with this one, after applying the
			// opacity; an RGB color would be read back as opaque
			aiColor4D const diffuse{to_linear(paint.color.r), to_linear(paint.color.g), to_linear(paint.color.b),
			                        opacity};
			material->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);

			// dielectrics reflect white highlights, metals reflect their own color
			auto const metallic = std::clamp(paint.metallic, 0.0f, 1.0f);
			auto const mix = [metallic](float color) { return 1.0f + (color - 1.0f) * metallic; };
			aiColor3D const specular{mix(diffuse.r), mix(diffuse.g), mix(diffuse.b)};
			material->AddProperty(&specular, 1, AI_MATKEY_COLOR_SPECULAR);

			auto const roughness = std::clamp(paint.roughness, 0.0f, 1.0f);

			// inverse of the glTF 2 exporter, which turns shininess back into
			// roughness as 1 - sqrt(shininess / 1000)
			auto const smoothness = 1.0f - roughness;
			auto const shininess = 1000.0f * smoothness * smoothness;
			material->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);

			// metallic/roughness workflow, read by the glTF 2 exporter before
			// it falls back to approximating the values from specular and
			// shininess; the exporter multiplies the alpha by the opacity
			aiColor4D const base_color{diffuse.r, diffuse.g, diffuse.b, 1.0f};
			material->AddProperty(&base_color, 1, AI_MATKEY_BASE_COLOR);
			material->AddProperty(&metallic, 1, AI_MATKEY_METALLIC_FACTOR);
			material->AddProperty(&roughness, 1, AI_MATKEY_ROUGHNESS_FACTOR);

			return material;
		}

		// creates the materials on first use, so only those used by the
		// meshes end up in the scene
		class material_table {
		public:
			explicit material_table(materials const& repo) : repo_{repo} {}

			unsigned index_of(std::string const& name) {
				if (auto const it = indices_.find(name); it != indices_.end()) {
					return it->second;
				}

				auto const found = repo_.lookup(name);
				auto const* paint = std::get_if<materials::paint_opts>(&found);
				auto const index = paint ? add(name, *paint) : missing();
				indices_.emplace(name, index);
				return index;
			}

			std::vector<aiMaterial*>& list() noexcept { return list_; }

		private:
			unsigned add(std::string const& name, materials::paint_opts const& paint) {
				auto const index = static_cast<unsigned>(list_.size());
				list_.push_back(make_material(name, paint));
				return index;
			}

			unsigned missing() {
				if (!missing_) missing_ = add(missing_material_name, missing_material);
				return *missing_;
			}

			materials const& repo_;
			std::vector<aiMaterial*> list_{};
			std::map<std::string, unsigned, std::less<>> indices_{};
			std::optional<unsigned> missing_{};
		};

		// Newell's method, on the doubles the sink keeps; counter-clockwise
		// faces point towards the viewer; empty for faces with no area
		std::optional<glm::dvec3> face_normal(std::vector<glm::dvec3> const& vertices,
		                                      sink::glm_solid::face_t const& face) {
			glm::dvec3 normal{};
			for (size_t corner = 0; corner < face.size(); ++corner) {
				auto const& current = vertices[face[corner]];
				auto const& next = vertices[face[(corner + 1) % face.size()]];
				normal.x += (current.y - next.y) * (current.z + next.z);
				normal.y += (current.z - next.z) * (current.x + next.x);
				normal.z += (current.x - next.x) * (current.y + next.y);
			}
			auto const length = glm::length(normal);
			if (!(length > 0.0)) return std::nullopt;
			return normal / length;
		}

		// The solids share vertices between faces, but a flat-shaded export
		// needs a normal per face: every face gets its own copies of its
		// vertices, all with the normal of the face. (Assimp's GenNormals
		// would not do it; with shared vertices, the last face touching
		// a vertex wins, leaving most normals lying in the faces.)
		aiMesh* make_mesh(std::string const& name, sink::glm_solid const& solid, unsigned material) {
			auto* mesh = new aiMesh{};
			mesh->mName = name;
			mesh->mMaterialIndex = material;

			auto const& vertices = solid.vertices();
			auto const vertex_count = vertices.size();

			struct flat_face {
				sink::glm_solid::face_t const* corners;
				glm::dvec3 normal;
			};
			std::vector<flat_face> faces{};
			faces.reserve(solid.faces().size());
			size_t corner_count = 0;
			for (auto const& face : solid.faces()) {
				if (face.size() < 3) continue;
				// faces with an index past the last vertex are dropped, as the
				// sink promises
				if (std::ranges::any_of(face, [=](size_t index) { return index >= vertex_count; })) {
					continue;
				}
				auto const normal = face_normal(vertices, face);
				if (!normal) continue;
				faces.push_back({&face, *normal});
				corner_count += face.size();
			}

			mesh->mNumVertices = static_cast<unsigned>(corner_count);
			mesh->mVertices = new aiVector3D[corner_count];
			mesh->mNormals = new aiVector3D[corner_count];
			mesh->mNumFaces = static_cast<unsigned>(faces.size());
			mesh->mFaces = new aiFace[faces.size()];

			unsigned next_vertex = 0;
			for (size_t index = 0; index < faces.size(); ++index) {
				auto const& [corners, normal] = faces[index];
				auto& dst = mesh->mFaces[index];
				dst.mNumIndices = static_cast<unsigned>(corners->size());
				dst.mIndices = new unsigned[corners->size()];
				aiVector3D const ai_normal{static_cast<ai_real>(normal.x), static_cast<ai_real>(normal.y),
				                           static_cast<ai_real>(normal.z)};
				for (size_t corner = 0; corner < corners->size(); ++corner) {
					auto const& v = vertices[(*corners)[corner]];
					mesh->mVertices[next_vertex] = {static_cast<ai_real>(v.x), static_cast<ai_real>(v.y),
					                                static_cast<ai_real>(v.z)};
					mesh->mNormals[next_vertex] = ai_normal;
					dst.mIndices[corner] = next_vertex++;
				}
				mesh->mPrimitiveTypes |= corners->size() == 3 ? aiPrimitiveType_TRIANGLE : aiPrimitiveType_POLYGON;
			}
			return mesh;
		}

		std::string path_name(sink::strings const& path) {
			std::string result{};
			for (auto const& name : path) {
				if (!result.empty()) result += '/';
				result += name;
			}
			return result;
		}

		std::unique_ptr<aiScene> make_scene(std::string const& name, sink const& out, materials const& repo) {
			material_table table{repo};
			std::vector<aiMesh*> meshes{};
			node_tree tree{};

			for (auto const& [material, by_path] : out.solids()) {
				for (auto const& [path, solid] : by_path) {
					auto* mesh = make_mesh(path_name(path), solid, table.index_of(material));
					if (mesh->mNumFaces == 0) {
						delete mesh;
						continue;
					}
					tree.at(path).meshes.push_back(static_cast<unsigned>(meshes.size()));
					meshes.push_back(mesh);
				}
			}

			// exporters expect at least one material
			auto& materials = table.list();
			if (materials.empty()) materials.push_back(make_material(missing_material_name, missing_material));

			// Assimp needs a single root node; it stays unnamed, so the groups
			// of the model look like the roots of the hierarchy (the model is
			// still named by the file)
			auto scene = std::make_unique<aiScene>();
			scene->mRootNode = tree.to_ai({}, nullptr);

			scene->mNumMaterials = static_cast<unsigned>(materials.size());
			scene->mMaterials = new aiMaterial*[materials.size()];
			std::copy(materials.begin(), materials.end(), scene->mMaterials);

			scene->mNumMeshes = static_cast<unsigned>(meshes.size());
			if (!meshes.empty()) {
				scene->mMeshes = new aiMesh*[meshes.size()];
				std::copy(meshes.begin(), meshes.end(), scene->mMeshes);
			}
			return scene;
		}
	}  // namespace

	std::vector<export_format> const& export_formats() {
		static std::vector<export_format> const formats = [] {
			std::vector<export_format> result{};
			Assimp::Exporter const exporter{};
			auto const count = exporter.GetExportFormatCount();
			result.reserve(count);
			for (size_t index = 0; index < count; ++index) {
				auto const* desc = exporter.GetExportFormatDescription(index);
				result.push_back({desc->id, desc->fileExtension, desc->description ? desc->description : ""});
			}
			return result;
		}();
		return formats;
	}

	void export_scene(std::string const& name,
	                  sink const& out,
	                  materials const& repo,
	                  std::string const& format_id,
	                  std::filesystem::path const& path) {
		auto const scene = make_scene(name, out, repo);

		Assimp::Exporter exporter{};
		// the meshes come with flat normals already (see make_mesh); the
		// n-gons are split into triangles, which keep the normals
		auto const result = exporter.Export(scene.get(), format_id, path.string(), aiProcess_Triangulate);
		if (result != aiReturn_SUCCESS) {
			throw std::runtime_error(
			    std::format("cannot export `{}` to {}: {}", name, path.string(), exporter.GetErrorString()));
		}
	}
}  // namespace shapes
