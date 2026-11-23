// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <assimp/Exporter.hpp>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <pyxel/base/str.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/io/interactions.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "gltf2.hpp"
#include "interactions_rules.hpp"

namespace pyxel::io {
	namespace {
		using gfx::light;
		using gfx::light_type;
		using gfx::materials;
		using gfx::sink;

		// glm is column-major (m[column][row]), Assimp is row-major
		aiMatrix4x4 to_ai(glm::dmat4 const& m) {
			auto const r = [](double value) { return static_cast<ai_real>(value); };
			return {r(m[0][0]), r(m[1][0]), r(m[2][0]), r(m[3][0]),  //
			        r(m[0][1]), r(m[1][1]), r(m[2][1]), r(m[3][1]),  //
			        r(m[0][2]), r(m[1][2]), r(m[2][2]), r(m[3][2]),  //
			        r(m[0][3]), r(m[1][3]), r(m[2][3]), r(m[3][3])};
		}

		// marks the node of a light for the glb2-pyxel and gltf2-pyxel
		// exporters, which read it back from the extras of the glTF node
		// (see gltf2.hpp); the size of an area light goes with the
		// aiLight, the rest of its surface has no place there
		struct light_tag {
			unsigned index{};
			std::optional<pyxel::gfx::length> range{};
			std::optional<light::area_t> area{};
		};

		light_tag make_tag(unsigned index, light const& source) {
			light_tag result{.index = index, .range = source.range};
			if (source.type == light_type::area) result.area = source.area;
			return result;
		}

		int32_t to_tag(pyxel::gfx::area_shape shape) noexcept {
			switch (shape) {
				case pyxel::gfx::area_shape::disk:
				case pyxel::gfx::area_shape::ellipse:
					return gltf2::area_ellipse;
				case pyxel::gfx::area_shape::square:
				case pyxel::gfx::area_shape::rectangle:
					break;
			}
			return gltf2::area_rectangle;
		}

		aiMetadata* make_metadata(light_tag const& tag) {
			auto* meta = new aiMetadata{};
			meta->Add(gltf2::light_key, static_cast<int32_t>(tag.index));
			if (tag.range) meta->Add(gltf2::range_key, static_cast<float>(tag.range->count()));
			if (tag.area) {
				meta->Add(gltf2::area_shape_key, to_tag(tag.area->shape));
				meta->Add(gltf2::area_spread_key, static_cast<float>(tag.area->spread.count()));
			}
			return meta;
		}

		// node created for a light, which cannot use the node it is in
		struct light_leaf {
			std::string name{};
			glm::dmat4 matrix{1.0};
			light_tag tag{};
		};

		// The nodes of the sink, with the indices of their meshes in the scene,
		// and the nodes made for their lights; a node is exported, if it has
		// a mesh or a light, or a descendant with one, or if an active object
		// names it (the root always is, Assimp needs it)
		struct node_tree {
			// in the order of declaration: the public members come first
			node_tree(sink const& out, bool tag_lights, std::set<std::string, std::less<>> const& moved)
			    : meshes(out.nodes().size())
			    , light_leaves(out.nodes().size())
			    , own_lights(out.nodes().size())
			    , out_{out}
			    , tag_lights_{tag_lights}
			    , moved_{moved} {}

			aiNode* to_ai() {
				auto const& nodes = out_.nodes();
				keep_.assign(nodes.size(), false);
				// children are opened after their parents, so they come later
				for (auto id = nodes.size(); id-- > 0;) {
					auto const named = !nodes[id].name.empty() && moved_.contains(nodes[id].name);
					if (meshes[id].empty() && nodes[id].lights.empty() && !named && !keep_[id]) continue;
					keep_[id] = true;
					if (auto const parent = nodes[id].parent) keep_[*parent] = true;
				}
				return to_ai(sink::root, nullptr);
			}

			std::vector<std::vector<unsigned>> meshes;
			std::vector<std::vector<light_leaf>> light_leaves;
			// the light using the node itself, if any
			std::vector<std::optional<light_tag>> own_lights;

		private:
			aiNode* to_ai(sink::node_id id, aiNode* parent) const {
				auto const& source = out_.at(id);
				auto* node = new aiNode{source.name};
				node->mParent = parent;
				node->mTransformation = io::to_ai(source.local);
				if (tag_lights_ && own_lights[id]) node->mMetaData = make_metadata(*own_lights[id]);

				auto const& own = meshes[id];
				node->mNumMeshes = static_cast<unsigned>(own.size());
				if (!own.empty()) {
					node->mMeshes = new unsigned[own.size()];
					std::copy(own.begin(), own.end(), node->mMeshes);
				}

				auto const& leaves = light_leaves[id];
				auto const kept = static_cast<unsigned>(
				    std::ranges::count_if(source.children, [this](sink::node_id child) { return keep_[child]; }));
				auto const child_count = kept + static_cast<unsigned>(leaves.size());
				node->mNumChildren = child_count;
				if (child_count != 0) {
					node->mChildren = new aiNode*[child_count];
					auto* dst = node->mChildren;
					for (auto const child : source.children) {
						if (keep_[child]) *dst++ = to_ai(child, node);
					}
					for (auto const& [name, matrix, tag] : leaves) {
						auto* leaf = new aiNode{name};
						leaf->mParent = node;
						leaf->mTransformation = io::to_ai(matrix);
						if (tag_lights_) leaf->mMetaData = make_metadata(tag);
						*dst++ = leaf;
					}
				}
				return node;
			}

			sink const& out_;
			bool tag_lights_;
			std::set<std::string, std::less<>> const& moved_;
			std::vector<bool> keep_{};
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
			auto const past_the_end = [=](size_t index) { return index >= vertex_count; };

			size_t corner_count = 0;
			for (auto const& face : solid.faces()) {
				if (face.size() < 3) continue;
				// faces with an index past the last vertex are dropped, as the
				// sink promises
				if (std::ranges::any_of(face, past_the_end)) continue;
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
				// aiPrimitiveType is a plain enum, which would be promoted to int
				mesh->mPrimitiveTypes |=
				    std::to_underlying(corners->size() == 3 ? aiPrimitiveType_TRIANGLE : aiPrimitiveType_POLYGON);
			}
			return mesh;
		}

		// names of the node and of its ancestors, root first, e.g. `house/door`
		std::string path_name(sink const& out, sink::node_id id) {
			std::string result{};
			for (std::optional<sink::node_id> next = id; next; next = out.at(*next).parent) {
				auto const& name = out.at(*next).name;
				if (name.empty()) continue;
				result = result.empty() ? name : name + '/' + result;
			}
			return result;
		}

		aiColor3D to_linear(pyxel::gfx::color const& color, float factor) noexcept {
			return {to_linear(color.r) * factor, to_linear(color.g) * factor, to_linear(color.b) * factor};
		}

		aiLightSourceType to_ai(light_type type) noexcept {
			switch (type) {
				case light_type::directional:
					return aiLightSource_DIRECTIONAL;
				case light_type::spot:
					return aiLightSource_SPOT;
				case light_type::area:
					return aiLightSource_AREA;
				case light_type::point:
					break;
			}
			return aiLightSource_POINT;
		}

		// The reverse of what Assimp's glTF 2 importer does with
		// KHR_lights_punctual: the intensity is folded into the colors, the
		// falloff is 1 / d^2 (the range has no place in aiLight, and is lost),
		// the light shines along -z of its node, and the cone angles go from
		// the axis to the edge. Not every exporter writes every light: of
		// Assimp's own, none writes the area lights
		aiLight* make_light(std::string const& name, light const& source) {
			auto* result = new aiLight{};
			result->mName = name;
			result->mType = to_ai(source.type);
			result->mPosition = {0, 0, 0};
			result->mDirection = {0, 0, -1};
			result->mUp = {0, 1, 0};

			auto const color = to_linear(source.color, source.intensity);
			result->mColorDiffuse = color;
			result->mColorSpecular = color;

			if (source.type == light_type::directional) {
				result->mAttenuationConstant = 1;
				result->mAttenuationLinear = 0;
				result->mAttenuationQuadratic = 0;
			} else {
				result->mAttenuationConstant = 0;
				result->mAttenuationLinear = 0;
				result->mAttenuationQuadratic = 1;
			}

			if (source.type == light_type::spot) {
				result->mAngleInnerCone = static_cast<float>(source.cone.inner_angle.count());
				result->mAngleOuterCone = static_cast<float>(source.cone.outer_angle.count());
			}

			if (source.type == light_type::area) {
				result->mSize = {static_cast<ai_real>(source.area.width.count()),
				                 static_cast<ai_real>(source.area.effective_height().count())};
			}
			return result;
		}

		// the name a light was declared with, and the name of the node it is
		// written on, which is its name on import
		struct written_light {
			sink::node_id node{};
			std::string declared{};
			std::string written{};
		};

		struct lights_result {
			std::vector<aiLight*> lights{};
			std::vector<written_light> names{};
		};

		// The exporters find the node of a light by its name, and the importer
		// takes the name of that node as the name of the light, so a light is
		// written on a node named after it, whenever it can be (see Names in
		// docs/model-interactions.md). A light with an identity matrix, added
		// in a node of its name, takes that node, as does such a light with
		// no name, in a named node, whose name no light takes; that needs the
		// name of the node to be used by no other node, and the node to have
		// no other light and no meshes (the Collada and FBX exporters give
		// a node either meshes or a light, never both). Any other light gets a new child node, placed
		// by the light's matrix, named after the light, if no node uses that
		// name yet. A light with no name, or with a name already used, gets
		// a generated one, `Light`, `Light.001`, ..., which neither a node nor
		// a light of the model uses.
		lights_result make_lights(sink const& out, node_tree& tree) {
			auto const& nodes = out.nodes();

			std::map<std::string, size_t, std::less<>> name_counts{};
			for (auto const& node : nodes)
				++name_counts[node.name];

			// the names of the nodes written so far; a generated name takes
			// none of them, and none of the names of the lights either
			std::set<std::string, std::less<>> used{};
			for (auto const& [name, count] : name_counts)
				used.insert(name);
			std::set<std::string, std::less<>> light_names{};
			for (auto const& node : nodes) {
				for (auto const& placed : node.lights) {
					if (!placed.light.name.empty()) light_names.insert(placed.light.name);
				}
			}

			unsigned counter = 0;
			auto const generated = [&] {
				std::string name{"Light"};
				while (light_names.contains(name) || used.contains(name))
					name = std::format("Light.{:03}", ++counter);
				used.insert(name);
				return name;
			};

			std::set<sink::node_id> claimed{};
			lights_result result{};
			for (sink::node_id id = 0; id < nodes.size(); ++id) {
				auto const& node = nodes[id];
				for (auto const& [source, matrix] : node.lights) {
					auto const tag = make_tag(static_cast<unsigned>(result.lights.size()), source);
					auto const named = !source.name.empty();
					auto const own_node = matrix == glm::dmat4{1.0} && !node.name.empty() &&
					                      (named ? node.name == source.name : !light_names.contains(node.name)) &&
					                      name_counts[node.name] == 1 && tree.meshes[id].empty() &&
					                      !claimed.contains(id);
					if (own_node) {
						claimed.insert(id);
						tree.own_lights[id] = tag;
						result.lights.push_back(make_light(node.name, source));
						result.names.push_back({.node = id, .declared = source.name, .written = node.name});
						continue;
					}

					std::string name{};
					if (named && !used.contains(source.name)) {
						name = source.name;
						used.insert(name);
					} else {
						name = generated();
					}
					tree.light_leaves[id].push_back({.name = name, .matrix = matrix, .tag = tag});
					result.lights.push_back(make_light(name, source));
					result.names.push_back({.node = id, .declared = source.name, .written = std::move(name)});
				}
			}
			return result;
		}

		struct scene_result {
			std::unique_ptr<aiScene> scene{};
			std::vector<written_light> lights{};
		};

		scene_result make_scene(sink const& out,
		                        materials const& repo,
		                        bool tag_lights,
		                        std::set<std::string, std::less<>> const& moved) {
			material_table table{repo};
			std::vector<aiMesh*> meshes{};
			node_tree tree{out, tag_lights, moved};

			for (sink::node_id id = 0; id < out.nodes().size(); ++id) {
				for (auto const& [material, solid] : out.at(id).meshes) {
					auto* mesh = make_mesh(path_name(out, id), solid, table.index_of(material));
					if (mesh->mNumFaces == 0) {
						delete mesh;
						continue;
					}
					tree.meshes[id].push_back(static_cast<unsigned>(meshes.size()));
					meshes.push_back(mesh);
				}
			}

			auto [lights, names] = make_lights(out, tree);

			// exporters expect at least one material
			auto& materials = table.list();
			if (materials.empty()) materials.push_back(make_material(missing_material_name, missing_material));

			// Assimp needs a single root node; it stays unnamed, so the groups
			// of the model look like the roots of the hierarchy (the model is
			// still named by the file)
			auto scene = std::make_unique<aiScene>();
			scene->mRootNode = tree.to_ai();

			scene->mNumMaterials = static_cast<unsigned>(materials.size());
			scene->mMaterials = new aiMaterial*[materials.size()];
			std::copy(materials.begin(), materials.end(), scene->mMaterials);

			scene->mNumMeshes = static_cast<unsigned>(meshes.size());
			if (!meshes.empty()) {
				scene->mMeshes = new aiMesh*[meshes.size()];
				std::copy(meshes.begin(), meshes.end(), scene->mMeshes);
			}

			scene->mNumLights = static_cast<unsigned>(lights.size());
			if (!lights.empty()) {
				scene->mLights = new aiLight*[lights.size()];
				std::copy(lights.begin(), lights.end(), scene->mLights);
			}
			return {.scene = std::move(scene), .lights = std::move(names)};
		}
		namespace decl = gfx::interactions;

		bool has_active_objects(sink const& out) {
			return std::ranges::any_of(out.nodes(), [](auto const& node) { return !node.active_objects.empty(); });
		}

		// The declarations of every node, in the order of the tree: a node is
		// opened after its parent and its earlier siblings. A light written
		// with a generated name (see make_lights) takes the object describing
		// it along only, if that object was added in the node of the light:
		// the object is renamed after the light. Any other object naming such
		// a light is dropped, unless another light was written with that
		// name; the importer would not find the light it describes.
		decl::active_objects collect_active_objects(sink const& out, std::vector<written_light> const& lights) {
			std::set<std::string, std::less<>> written{};
			std::set<std::string, std::less<>> renamed{};
			std::vector<bool> taken(lights.size(), false);
			for (auto const& light : lights) {
				written.insert(light.written);
				if (!light.declared.empty() && light.declared != light.written) renamed.insert(light.declared);
			}

			// the next renamed light of `name`, added in `node`, which no other
			// object of that node took yet
			auto const renamed_in = [&](sink::node_id node, std::string_view name) -> std::string const* {
				for (size_t index = 0; index < lights.size(); ++index) {
					auto const& light = lights[index];
					if (taken[index] || light.node != node || light.declared != name || light.written == name) continue;
					taken[index] = true;
					return &light.written;
				}
				return nullptr;
			};

			decl::active_objects result{};
			auto const& nodes = out.nodes();
			for (sink::node_id id = 0; id < nodes.size(); ++id) {
				for (auto const& item : nodes[id].active_objects) {
					auto const* light = std::get_if<decl::light_active_object>(&item);
					if (!light || !renamed.contains(light->name)) {
						result.push_back(item);
						continue;
					}
					if (auto const* name = renamed_in(id, light->name)) {
						result.push_back(decl::light_active_object{.name = *name, .light = light->light});
					} else if (written.contains(light->name)) {
						result.push_back(item);
					}
				}
			}
			return result;
		}

		std::string_view name_of(decl::active_object const& item) {
			return std::visit([](auto const& object) -> std::string_view { return object.name; }, item);
		}

		// The rules of docs/model-interactions.md (see Rules there), checked
		// on the whole model, with the lights under the names they are written
		// with, as the importer will see them. Lights sharing a name keep it
		// here, so the model fails, even though they are written under names
		// of their own; a light with no name stays out, as it is not an
		// active object. Any violation throws std::runtime_error, naming the
		// model, and every problem found, with its object.
		void check_active_objects(std::string const& name,
		                          decl::active_objects const& objects,
		                          sink const& out,
		                          std::vector<written_light> const& lights) {
			interactions::model_names model{};
			for (auto const& node : out.nodes())
				model.add_node(node.name);

			std::map<std::string_view, size_t> declared{};
			for (auto const& light : lights)
				++declared[light.declared];
			for (auto const& light : lights) {
				if (light.declared.empty()) continue;
				model.add_light(declared[light.declared] > 1 ? light.declared : light.written);
			}

			auto const problems = interactions::check(objects, model);
			if (problems.empty()) return;

			std::string message = std::format("cannot export `{}`:", name);
			for (auto const& [object, reason] : problems) {
				if (object) {
					message += std::format("\n- active object `{}` {}", name_of(objects[*object]), reason);
				} else {
					message += std::format("\n- {}", reason);
				}
			}
			throw std::runtime_error(message);
		}

		// the groups the geometry-based objects name, in their states and
		// transitions; their nodes are kept, even with nothing in them
		std::set<std::string, std::less<>> moved_groups(decl::active_objects const& objects) {
			std::set<std::string, std::less<>> result{};
			for (auto const& item : objects) {
				auto const* geo = std::get_if<decl::geo_active_object>(&item);
				if (!geo) continue;
				for (auto const& state : geo->states) {
					for (auto const& [group, changes] : state.groups)
						result.insert(group);
				}
				for (auto const& [group, rule] : geo->transitions.groups)
					result.insert(group);
			}
			return result;
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
		Assimp::Exporter exporter{};
		gltf2::register_exporters(exporter);

		// glTF 2, binary or text, with the lights added; the light nodes are
		// tagged for them, the other formats do not need the tags
		std::string const actual_format{gltf2::exporter_for(format_id)};
		auto const is_gltf2 = actual_format == gltf2::bin_format_id || actual_format == gltf2::text_format_id;

		// only the glTF 2 exporters can carry the interactions
		if (has_active_objects(out) && !is_gltf2) {
			throw std::runtime_error(std::format("cannot export `{}` to {}: a model with interactions needs {} or {}",
			                                     name, format_id, gltf2::bin_base_format_id,
			                                     gltf2::text_base_format_id));
		}

		// the groups moved by the geometry-based objects do not depend on the
		// names of the lights; the objects describing lights do
		auto const [scene, lights] = make_scene(out, repo, is_gltf2, moved_groups(collect_active_objects(out, {})));
		auto const objects = collect_active_objects(out, lights);

		Assimp::ExportProperties properties{};
		if (!objects.empty()) {
			check_active_objects(name, objects, out, lights);
			properties.SetPropertyString(gltf2::interactions_property, interactions::serialize(objects));
		}

		// the meshes come with flat normals already (see make_mesh); the
		// n-gons are split into triangles, which keep the normals
		// Assimp takes the path as UTF-8 (on Windows, it converts it to
		// UTF-16); path::string() would use the ANSI code page there
		auto const path_u8 = as_str(path.u8string());
		auto const result = exporter.Export(scene.get(), actual_format, path_u8, aiProcess_Triangulate, &properties);
		if (result != aiReturn_SUCCESS) {
			throw std::runtime_error(
			    std::format("cannot export `{}` to {}: {}", name, path_u8, exporter.GetErrorString()));
		}
	}
}  // namespace pyxel::io
