// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "gltf2.hpp"
#include <assimp/BaseImporter.h>
#include <assimp/Exceptional.h>
#include <assimp/importerdesc.h>
#include <assimp/scene.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <assimp/DefaultLogger.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "interactions_json.hpp"
#include "interactions_rules.hpp"

namespace pyxel::io::gltf2 {
	namespace {
		static_assert(std::endian::native == std::endian::little, "GLB is little-endian");

		constexpr auto punctual_extension = "KHR_lights_punctual";
		constexpr auto area_extension = "PYXEL_lights_area";

		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-gltf-layout
		constexpr std::uint32_t glb_magic = 0x46546C67;  // "glTF"
		constexpr std::uint32_t glb_version = 2;
		constexpr std::uint32_t chunk_json = 0x4E4F534A;  // "JSON"
		constexpr size_t header_size = 12;
		constexpr size_t chunk_header_size = 8;

		[[noreturn]] void fail(char const* exporter, std::string const& message) {
			throw DeadlyExportError(std::format("{}: {}", exporter, message));
		}

		[[noreturn]] void fail(std::string const& message) { fail(bin_format_id, message); }

		std::uint32_t read_u32(std::span<std::byte const> bytes, size_t offset) {
			if (offset + 4 > bytes.size()) fail("the binary from the glTF 2 exporter is cut short");
			std::uint32_t result{};
			std::memcpy(&result, bytes.data() + offset, sizeof(result));
			return result;
		}

		void write_u32(std::vector<std::byte>& out, std::uint32_t value) {
			auto const* bytes = reinterpret_cast<std::byte const*>(&value);
			out.insert(out.end(), bytes, bytes + sizeof(value));
		}

		struct chunk {
			std::uint32_t type;
			std::span<std::byte const> data;
		};

		std::vector<chunk> read_chunks(std::span<std::byte const> bytes) {
			if (read_u32(bytes, 0) != glb_magic || read_u32(bytes, 4) != glb_version) {
				fail("the glTF 2 exporter did not produce a GLB 2 binary");
			}
			auto const total = std::min<size_t>(read_u32(bytes, 8), bytes.size());

			std::vector<chunk> result{};
			for (size_t offset = header_size; offset + chunk_header_size <= total;) {
				auto const length = read_u32(bytes, offset);
				auto const type = read_u32(bytes, offset + 4);
				offset += chunk_header_size;
				if (offset + length > total) fail("a chunk of the binary is cut short");
				result.push_back({type, bytes.subspan(offset, length)});
				offset += length;
			}
			if (result.empty() || result.front().type != chunk_json) fail("the binary does not start with JSON");
			return result;
		}

		using allocator = rapidjson::Document::AllocatorType;

		rapidjson::Value& member_object(rapidjson::Value& parent, char const* name, allocator& alloc) {
			auto it = parent.FindMember(name);
			if (it == parent.MemberEnd() || !it->value.IsObject()) {
				if (it != parent.MemberEnd()) parent.RemoveMember(it);
				parent.AddMember(rapidjson::Value{name, alloc}, rapidjson::Value{rapidjson::kObjectType}, alloc);
				it = parent.FindMember(name);
			}
			return it->value;
		}

		rapidjson::Value number_array(std::initializer_list<double> values, allocator& alloc) {
			rapidjson::Value result{rapidjson::kArrayType};
			for (auto value : values)
				result.PushBack(value, alloc);
			return result;
		}

		// The reverse of Assimp's glTF 2 importer: the intensity was folded
		// into the colors, so it is split again, with the brightest channel
		// at 1
		void add_color(rapidjson::Value& light, aiColor3D const& color, allocator& alloc) {
			auto const intensity = std::max({color.r, color.g, color.b, 0.0f});
			if (intensity > 0.0f) {
				light.AddMember("color",
				                number_array({color.r / intensity, color.g / intensity, color.b / intensity}, alloc),
				                alloc);
			} else {
				light.AddMember("color", number_array({1.0, 1.0, 1.0}, alloc), alloc);
			}
			light.AddMember("intensity", static_cast<double>(intensity), alloc);
		}

		// what the export put into the extras of a node
		struct light_tags {
			int index{-1};
			std::optional<double> range{};
			std::optional<int> area_shape{};
			std::optional<double> area_spread{};
		};

		std::optional<double> number(rapidjson::Value const& extras, char const* key) {
			auto const it = extras.FindMember(key);
			if (it == extras.MemberEnd() || !it->value.IsNumber()) return std::nullopt;
			return it->value.GetDouble();
		}

		// reads the tags and removes them; the tags are ours, not of the model
		std::optional<light_tags> take_tags(rapidjson::Value& node) {
			auto const extras_it = node.FindMember("extras");
			if (extras_it == node.MemberEnd() || !extras_it->value.IsObject()) return std::nullopt;
			auto& extras = extras_it->value;

			auto const tag_it = extras.FindMember(light_key);
			if (tag_it == extras.MemberEnd()) return std::nullopt;

			light_tags result{};
			if (tag_it->value.IsInt()) result.index = tag_it->value.GetInt();
			result.range = number(extras, range_key);
			if (auto const it = extras.FindMember(area_shape_key); it != extras.MemberEnd() && it->value.IsInt()) {
				result.area_shape = it->value.GetInt();
			}
			result.area_spread = number(extras, area_spread_key);

			for (auto const key : {light_key, range_key, area_shape_key, area_spread_key})
				extras.RemoveMember(key);
			if (extras.ObjectEmpty()) node.RemoveMember("extras");
			return result;
		}

		void add_range(rapidjson::Value& light, std::optional<double> range, allocator& alloc) {
			if (range && *range > 0.0) light.AddMember("range", *range, alloc);
		}

		// KHR_lights_punctual; directional, point and spot lights only
		std::optional<rapidjson::Value> make_punctual(aiLight const& source, light_tags const& tags, allocator& alloc) {
			char const* type = nullptr;
			switch (source.mType) {
				case aiLightSource_DIRECTIONAL:
					type = "directional";
					break;
				case aiLightSource_POINT:
					type = "point";
					break;
				case aiLightSource_SPOT:
					type = "spot";
					break;
				default:
					return std::nullopt;
			}

			rapidjson::Value light{rapidjson::kObjectType};
			light.AddMember("name", rapidjson::Value{source.mName.C_Str(), alloc}, alloc);
			light.AddMember("type", rapidjson::StringRef(type), alloc);
			add_color(light, source.mColorDiffuse, alloc);

			if (source.mType == aiLightSource_SPOT) {
				// KHR_lights_punctual: 0 <= inner < outer <= pi / 2
				constexpr double right_angle = std::numbers::pi / 2;
				constexpr double smallest = 1e-4;
				auto const outer = std::clamp<double>(source.mAngleOuterCone, smallest, right_angle);
				auto inner = std::clamp<double>(source.mAngleInnerCone, 0.0, outer);
				// a hard edge, as close as the extension allows
				if (inner >= outer) inner = outer - smallest;

				rapidjson::Value spot{rapidjson::kObjectType};
				spot.AddMember("innerConeAngle", inner, alloc);
				spot.AddMember("outerConeAngle", outer, alloc);
				light.AddMember("spot", spot, alloc);
			}

			if (source.mType != aiLightSource_DIRECTIONAL) add_range(light, tags.range, alloc);
			return light;
		}

		// PYXEL_lights_area (see gltf2.hpp)
		rapidjson::Value make_area(aiLight const& source, light_tags const& tags, allocator& alloc) {
			rapidjson::Value light{rapidjson::kObjectType};
			light.AddMember("name", rapidjson::Value{source.mName.C_Str(), alloc}, alloc);

			auto const* shape = tags.area_shape.value_or(area_rectangle) == area_ellipse ? "ellipse" : "rectangle";
			light.AddMember("shape", rapidjson::StringRef(shape), alloc);
			light.AddMember("width", static_cast<double>(source.mSize.x), alloc);
			light.AddMember("height", static_cast<double>(source.mSize.y), alloc);
			add_color(light, source.mColorDiffuse, alloc);
			light.AddMember("spread", std::clamp(tags.area_spread.value_or(std::numbers::pi), 0.0, std::numbers::pi),
			                alloc);
			add_range(light, tags.range, alloc);
			return light;
		}

		void use_extension(rapidjson::Document& doc, char const* name) {
			auto& alloc = doc.GetAllocator();
			auto used_it = doc.FindMember("extensionsUsed");
			if (used_it == doc.MemberEnd() || !used_it->value.IsArray()) {
				if (used_it != doc.MemberEnd()) doc.RemoveMember(used_it);
				doc.AddMember("extensionsUsed", rapidjson::Value{rapidjson::kArrayType}, alloc);
				used_it = doc.FindMember("extensionsUsed");
			}
			auto& used = used_it->value;
			auto const listed = std::ranges::any_of(used.GetArray(), [name](rapidjson::Value const& item) {
				return item.IsString() && std::string_view{item.GetString()} == name;
			});
			if (!listed) used.PushBack(rapidjson::StringRef(name), alloc);
		}

		// lights of one extension, gathered from the nodes
		struct light_list {
			char const* extension;
			rapidjson::Value lights{rapidjson::kArrayType};

			void add(rapidjson::Value& node, rapidjson::Value light, allocator& alloc) {
				rapidjson::Value ref{rapidjson::kObjectType};
				ref.AddMember("light", lights.Size(), alloc);
				member_object(node, "extensions", alloc).AddMember(rapidjson::StringRef(extension), ref, alloc);
				lights.PushBack(light, alloc);
			}

			void finish(rapidjson::Document& doc) {
				if (lights.Empty()) return;
				auto& alloc = doc.GetAllocator();
				rapidjson::Value ext{rapidjson::kObjectType};
				ext.AddMember("lights", lights, alloc);
				member_object(doc, "extensions", alloc).AddMember(rapidjson::StringRef(extension), ext, alloc);
				use_extension(doc, extension);
			}
		};

		void add_lights(rapidjson::Document& doc, aiScene const& scene) {
			auto& alloc = doc.GetAllocator();
			light_list punctual{punctual_extension};
			light_list area{area_extension};

			auto const nodes_it = doc.FindMember("nodes");
			if (nodes_it != doc.MemberEnd() && nodes_it->value.IsArray()) {
				for (auto& node : nodes_it->value.GetArray()) {
					if (!node.IsObject()) continue;
					auto const tags = take_tags(node);
					if (!tags || tags->index < 0 || static_cast<unsigned>(tags->index) >= scene.mNumLights) continue;

					auto const& source = *scene.mLights[tags->index];
					if (source.mType == aiLightSource_AREA) {
						area.add(node, make_area(source, *tags, alloc), alloc);
					} else if (auto light = make_punctual(source, *tags, alloc)) {
						punctual.add(node, std::move(*light), alloc);
					}
				}
			}

			punctual.finish(doc);
			area.finish(doc);
		}

		// the first root node of the default scene (or of the first one)
		rapidjson::Value* root_node(rapidjson::Document& doc) {
			auto const scenes_it = doc.FindMember("scenes");
			auto const nodes_it = doc.FindMember("nodes");
			if (scenes_it == doc.MemberEnd() || !scenes_it->value.IsArray() || scenes_it->value.Empty() ||
			    nodes_it == doc.MemberEnd() || !nodes_it->value.IsArray())
				return nullptr;

			auto const scene_it = doc.FindMember("scene");
			auto const scene_index =
			    scene_it != doc.MemberEnd() && scene_it->value.IsUint() ? scene_it->value.GetUint() : 0u;
			if (scene_index >= scenes_it->value.Size()) return nullptr;

			auto const& scene = scenes_it->value[scene_index];
			if (!scene.IsObject()) return nullptr;
			auto const roots_it = scene.FindMember("nodes");
			if (roots_it == scene.MemberEnd() || !roots_it->value.IsArray() || roots_it->value.Empty() ||
			    !roots_it->value[0].IsUint())
				return nullptr;

			auto const index = roots_it->value[0].GetUint();
			if (index >= nodes_it->value.Size() || !nodes_it->value[index].IsObject()) return nullptr;
			return &nodes_it->value[index];
		}

		void add_interactions(char const* exporter,
		                      rapidjson::Document& doc,
		                      Assimp::ExportProperties const* properties) {
			if (!properties || !properties->HasPropertyString(interactions_property)) return;
			auto const text = properties->GetPropertyString(interactions_property);
			if (text.empty()) return;

			auto& alloc = doc.GetAllocator();
			rapidjson::Document extra{&alloc};
			extra.Parse(text.data(), text.size());
			if (extra.HasParseError()) {
				fail(exporter, std::format("the {} extra cannot be parsed: {}", interactions_extra,
				                           rapidjson::GetParseError_En(extra.GetParseError())));
			}

			auto* root = root_node(doc);
			if (!root) fail(exporter, std::format("no root node to put the {} extra in", interactions_extra));

			// parsed with the allocator of the document, so it can be moved in
			auto& extras = member_object(*root, "extras", alloc);
			extras.RemoveMember(interactions_extra);
			extras.AddMember(rapidjson::StringRef(interactions_extra), extra.Move(), alloc);
		}

		// the JSON document from Assimp, with the lights and the
		// interactions added
		rapidjson::Document parse_with_extras(char const* exporter,
		                                      std::string_view json,
		                                      aiScene const& scene,
		                                      Assimp::ExportProperties const* properties) {
			rapidjson::Document doc{};
			doc.Parse(json.data(), json.size());
			if (doc.HasParseError() || !doc.IsObject()) fail(exporter, "the JSON document cannot be parsed");

			add_lights(doc, scene);
			add_interactions(exporter, doc, properties);
			return doc;
		}

		void write_file(char const* exporter,
		                Assimp::IOSystem& io,
		                char const* path,
		                char const* mode,
		                void const* data,
		                size_t size) {
			auto* stream = io.Open(path, mode);
			if (!stream) fail(exporter, std::format("cannot open {} for writing", path));
			auto const written = stream->Write(data, 1, size);
			io.Close(stream);
			if (written != size) fail(exporter, std::format("cannot write {}", path));
		}

		std::vector<std::byte> rebuild(std::span<std::byte const> blob,
		                               aiScene const& scene,
		                               Assimp::ExportProperties const* properties) {
			auto const chunks = read_chunks(blob);
			auto const& json = chunks.front().data;
			auto const doc = parse_with_extras(bin_format_id, {reinterpret_cast<char const*>(json.data()), json.size()},
			                                   scene, properties);

			rapidjson::StringBuffer text{};
			rapidjson::Writer<rapidjson::StringBuffer> writer{text};
			doc.Accept(writer);

			std::string json_text{text.GetString(), text.GetSize()};
			// chunks are aligned to 4 bytes; JSON is padded with spaces
			while (json_text.size() % 4 != 0)
				json_text.push_back(' ');

			std::vector<std::byte> out{};
			out.reserve(blob.size() + json_text.size());
			write_u32(out, glb_magic);
			write_u32(out, glb_version);
			write_u32(out, 0);  // total length, set below

			write_u32(out, static_cast<std::uint32_t>(json_text.size()));
			write_u32(out, chunk_json);
			auto const* json_bytes = reinterpret_cast<std::byte const*>(json_text.data());
			out.insert(out.end(), json_bytes, json_bytes + json_text.size());

			for (auto const& [type, data] : std::span{chunks}.subspan(1)) {
				write_u32(out, static_cast<std::uint32_t>(data.size()));
				write_u32(out, type);
				out.insert(out.end(), data.begin(), data.end());
			}

			auto const total = static_cast<std::uint32_t>(out.size());
			std::memcpy(out.data() + 8, &total, sizeof(total));
			return out;
		}

		void export_glb_pyxel(char const* path,
		                      Assimp::IOSystem* io,
		                      aiScene const* scene,
		                      Assimp::ExportProperties const* properties) {
			// the scene was post-processed already, by the exporter calling
			// this function
			Assimp::Exporter base{};
			auto const* blob = base.ExportToBlob(scene, bin_base_format_id, 0u, properties);
			if (!blob) fail(base.GetErrorString());

			auto const bytes = rebuild({static_cast<std::byte const*>(blob->data), blob->size}, *scene, properties);
			write_file(bin_format_id, *io, path, "wb", bytes.data(), bytes.size());
		}

		// writes into a string owned by someone else; Assimp's glTF 2 writer
		// deletes its streams, instead of closing them
		class memory_stream final : public Assimp::IOStream {
		public:
			explicit memory_stream(std::string& data) : data_{data} { data_.clear(); }

			size_t Read(void*, size_t, size_t) override { return 0; }

			size_t Write(void const* buffer, size_t size, size_t count) override {
				if (size == 0 || count == 0) return 0;
				auto const bytes = size * count;
				if (position_ + bytes > data_.size()) data_.resize(position_ + bytes);
				std::memcpy(data_.data() + position_, buffer, bytes);
				position_ += bytes;
				return count;
			}

			aiReturn Seek(size_t offset, aiOrigin origin) override {
				size_t base = 0;
				switch (origin) {
					case aiOrigin_SET:
						break;
					case aiOrigin_CUR:
						base = position_;
						break;
					case aiOrigin_END:
						base = data_.size();
						break;
					default:
						return aiReturn_FAILURE;
				}
				if (base + offset > data_.size()) return aiReturn_FAILURE;
				position_ = base + offset;
				return aiReturn_SUCCESS;
			}

			size_t Tell() const override { return position_; }
			size_t FileSize() const override { return data_.size(); }
			void Flush() override {}

		private:
			std::string& data_;
			size_t position_{};
		};

		// The file system of the exporter, except for the `.gltf` file itself,
		// which goes to `json`; the `.bin` files of the buffers are written
		// as they are
		class json_capture final : public Assimp::IOSystem {
		public:
			json_capture(Assimp::IOSystem& io, char const* path, std::string& json)
			    : io_{io}, path_{path}, json_{json} {}

			bool Exists(char const* file) const override { return io_.Exists(file); }
			char getOsSeparator() const override { return io_.getOsSeparator(); }

			Assimp::IOStream* Open(char const* file, char const* mode) override {
				if (io_.ComparePaths(file, path_.c_str())) return new memory_stream{json_};
				return io_.Open(file, mode);
			}

			void Close(Assimp::IOStream* file) override {
				if (dynamic_cast<memory_stream*>(file)) {
					delete file;
					return;
				}
				io_.Close(file);
			}

			bool ComparePaths(char const* one, char const* second) const override {
				return io_.ComparePaths(one, second);
			}
			bool PushDirectory(std::string const& path) override { return io_.PushDirectory(path); }
			std::string const& CurrentDirectory() const override { return io_.CurrentDirectory(); }
			size_t StackSize() const override { return io_.StackSize(); }
			bool PopDirectory() override { return io_.PopDirectory(); }
			bool CreateDirectory(std::string const& path) override { return io_.CreateDirectory(path); }
			bool ChangeDirectory(std::string const& path) override { return io_.ChangeDirectory(path); }
			bool DeleteFile(std::string const& file) override { return io_.DeleteFile(file); }

		private:
			Assimp::IOSystem& io_;
			std::string path_;
			std::string& json_;
		};

		void export_gltf_pyxel(char const* path,
		                       Assimp::IOSystem* io,
		                       aiScene const* scene,
		                       Assimp::ExportProperties const* properties) {
			// declared first: it outlives the exporter, which owns the file
			// system writing into it
			std::string json{};
			{
				// the scene was post-processed already, by the exporter
				// calling this function
				Assimp::Exporter base{};
				base.SetIOHandler(new json_capture{*io, path, json});
				if (base.Export(scene, text_base_format_id, path, 0u, properties) != aiReturn_SUCCESS) {
					fail(text_format_id, base.GetErrorString());
				}
			}
			if (json.empty()) fail(text_format_id, "the glTF 2 exporter wrote no JSON document");

			auto const doc = parse_with_extras(text_format_id, json, *scene, properties);

			// indented, as Assimp writes it
			rapidjson::StringBuffer text{};
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{text};
			doc.Accept(writer);
			write_file(text_format_id, *io, path, "wt", text.GetString(), text.GetSize());
		}

		// ---- importer ----

		void warn(std::string const& message) {
			Assimp::DefaultLogger::get()->warn(std::format("{}: {}", importer_name, message).c_str());
		}

		struct stream_closer {
			Assimp::IOSystem* io;
			void operator()(Assimp::IOStream* stream) const { io->Close(stream); }
		};

		bool read_exactly(Assimp::IOStream& stream, void* buffer, size_t size) {
			return size == 0 || stream.Read(buffer, 1, size) == size;
		}

		// the JSON document of a glTF 2 file: the `.gltf` file itself, or
		// the first chunk of a `.glb`
		std::optional<std::string> read_json(Assimp::IOSystem& io, std::string const& path) {
			std::unique_ptr<Assimp::IOStream, stream_closer> stream{io.Open(path, "rb"), stream_closer{&io}};
			if (!stream) return std::nullopt;
			auto const size = stream->FileSize();

			std::uint32_t header[3]{};
			if (size >= header_size && read_exactly(*stream, header, header_size) && header[0] == glb_magic) {
				std::uint32_t chunk[2]{};
				if (size < header_size + chunk_header_size || !read_exactly(*stream, chunk, chunk_header_size) ||
				    chunk[1] != chunk_json || chunk[0] > size - header_size - chunk_header_size) {
					return std::nullopt;
				}
				std::string result(chunk[0], '\0');
				if (!read_exactly(*stream, result.data(), result.size())) return std::nullopt;
				return result;
			}

			std::string result(size, '\0');
			if (stream->Seek(0, aiOrigin_SET) != aiReturn_SUCCESS || !read_exactly(*stream, result.data(), size)) {
				return std::nullopt;
			}
			// a byte order mark, which RapidJSON does not skip
			if (result.starts_with("\xEF\xBB\xBF")) result.erase(0, 3);
			return result;
		}

		rapidjson::Value const* member(rapidjson::Value const& parent, char const* name) {
			if (!parent.IsObject()) return nullptr;
			auto const it = parent.FindMember(name);
			return it == parent.MemberEnd() ? nullptr : &it->value;
		}

		rapidjson::Value const* extension_of(rapidjson::Value const& object, char const* name) {
			auto const* extensions = member(object, "extensions");
			return extensions ? member(*extensions, name) : nullptr;
		}

		std::optional<double> finite_number(rapidjson::Value const& object, char const* name) {
			auto const* value = member(object, name);
			if (!value || !value->IsNumber()) return std::nullopt;
			auto const result = value->GetDouble();
			if (!std::isfinite(result)) return std::nullopt;
			return result;
		}

		// an area light of PYXEL_lights_area, as read from the document
		struct area_source {
			int shape{area_rectangle};
			float width{};
			float height{};
			aiColor3D color{1.0f, 1.0f, 1.0f};
			float spread{std::numbers::pi_v<float>};
			std::optional<float> range{};
		};

		// the light, if it has a shape and a size; the rest has defaults
		std::optional<area_source> read_area(rapidjson::Value const& light, size_t index) {
			auto const failed = [index](std::string_view why) {
				warn(std::format("area light {} {}; left out", index, why));
				return std::nullopt;
			};

			if (!light.IsObject()) return failed("is not an object");

			area_source result{};
			auto const* shape = member(light, "shape");
			if (!shape || !shape->IsString()) return failed("has no shape");
			std::string_view const shape_name{shape->GetString(), shape->GetStringLength()};
			if (shape_name == "rectangle") {
				result.shape = area_rectangle;
			} else if (shape_name == "ellipse") {
				result.shape = area_ellipse;
			} else {
				return failed(std::format("has an unknown shape, \"{}\"", shape_name));
			}

			auto const width = finite_number(light, "width");
			auto const height = finite_number(light, "height");
			if (!width || !height || *width < 0.0 || *height < 0.0) return failed("has no size");
			result.width = static_cast<float>(*width);
			result.height = static_cast<float>(*height);

			// as in KHR_lights_punctual: white, of intensity 1, by default
			auto color = aiColor3D{1.0f, 1.0f, 1.0f};
			if (auto const* rgb = member(light, "color");
			    rgb && rgb->IsArray() && rgb->Size() == 3 &&
			    std::ranges::all_of(rgb->GetArray(), [](auto const& channel) { return channel.IsNumber(); })) {
				color = {(*rgb)[0].GetFloat(), (*rgb)[1].GetFloat(), (*rgb)[2].GetFloat()};
			}
			auto const intensity = static_cast<float>(finite_number(light, "intensity").value_or(1.0));
			result.color = color * intensity;

			if (auto const spread = finite_number(light, "spread")) {
				result.spread = static_cast<float>(std::clamp(*spread, 0.0, std::numbers::pi));
			}
			if (auto const range = finite_number(light, "range"); range && *range > 0.0) {
				result.range = static_cast<float>(*range);
			}
			return result;
		}

		// The area lights found in the nodes, with the aiNodes they belong
		// to. Assimp makes one aiNode of every glTF node of the scene, in
		// the same order, so both trees are walked together; the names are
		// not unique, and cannot be used for this
		class area_lights {
		public:
			explicit area_lights(rapidjson::Value const& doc) : doc_{doc} {
				if (auto const* ext = extension_of(doc, area_extension)) {
					if (auto const* lights = member(*ext, "lights"); lights && lights->IsArray()) lights_ = lights;
				}
				if (auto const* nodes = member(doc, "nodes"); nodes && nodes->IsArray()) nodes_ = nodes;
			}

			void add_to(aiScene& scene) {
				if (!lights_ || !nodes_ || !scene.mRootNode) return;

				auto const roots = indices(scene_roots());
				if (roots.size() == 1) {
					visit(roots.front(), *scene.mRootNode, 0);
				} else if (!roots.empty()) {
					visit_children(roots, *scene.mRootNode, 0);
				}
				if (found_.empty()) return;

				auto const count = scene.mNumLights + found_.size();
				auto* list = new aiLight*[count];
				std::copy(scene.mLights, scene.mLights + scene.mNumLights, list);
				for (size_t index = 0; index < found_.size(); ++index)
					list[scene.mNumLights + index] = found_[index].release();
				delete[] scene.mLights;
				scene.mLights = list;
				scene.mNumLights = static_cast<unsigned>(count);
			}

		private:
			// Assimp's limit is the stack; this one only keeps a document
			// with a cycle from overflowing it, should it get here
			static constexpr size_t max_depth = 1024;

			rapidjson::Value const* scene_roots() const {
				size_t index = 0;
				if (auto const* chosen = member(doc_, "scene"); chosen && chosen->IsUint()) index = chosen->GetUint();
				auto const* scenes = member(doc_, "scenes");
				if (!scenes || !scenes->IsArray() || index >= scenes->Size()) return nullptr;
				return member((*scenes)[static_cast<rapidjson::SizeType>(index)], "nodes");
			}

			// the node indices of the array, as Assimp reads them: only the
			// unsigned integers count; Assimp fails on the ones out of range
			std::vector<unsigned> indices(rapidjson::Value const* array) const {
				std::vector<unsigned> result{};
				if (!array || !array->IsArray()) return result;
				for (auto const& item : array->GetArray()) {
					if (item.IsUint() && item.GetUint() < nodes_->Size()) result.push_back(item.GetUint());
				}
				return result;
			}

			void visit_children(std::vector<unsigned> const& children, aiNode& parent, size_t depth) {
				if (children.size() != parent.mNumChildren) {
					warn(
					    std::format("the children of node `{}` do not match the document; their area lights are "
					                "left out",
					                parent.mName.C_Str()));
					return;
				}
				for (size_t index = 0; index < children.size(); ++index)
					visit(children[index], *parent.mChildren[index], depth + 1);
			}

			void visit(unsigned index, aiNode& target, size_t depth) {
				if (depth > max_depth) return;
				auto const& node = (*nodes_)[index];
				if (auto const* ext = extension_of(node, area_extension)) add_light(node, *ext, target);
				visit_children(indices(member(node, "children")), target, depth);
			}

			void add_light(rapidjson::Value const& node, rapidjson::Value const& ext, aiNode& target) {
				auto const* ref = member(ext, "light");
				if (!ref || !ref->IsUint() || ref->GetUint() >= lights_->Size()) {
					warn(std::format("node `{}` refers to no area light", target.mName.C_Str()));
					return;
				}
				auto const index = ref->GetUint();
				if (extension_of(node, punctual_extension)) {
					warn(std::format("node `{}` has a punctual light already; area light {} left out",
					                 target.mName.C_Str(), index));
					return;
				}
				auto const source = read_area((*lights_)[index], index);
				if (!source) return;

				auto light = std::make_unique<aiLight>();
				light->mName = target.mName;
				light->mType = aiLightSource_AREA;
				light->mPosition = {0, 0, 0};
				light->mDirection = {0, 0, -1};
				light->mUp = {0, 1, 0};
				light->mColorDiffuse = source->color;
				light->mColorSpecular = source->color;
				light->mColorAmbient = source->color;
				light->mAttenuationConstant = 0;
				light->mAttenuationLinear = 0;
				light->mAttenuationQuadratic = 1;
				light->mSize = {source->width, source->height};

				if (!target.mMetaData) target.mMetaData = new aiMetadata{};
				target.mMetaData->Add(area_shape_key, static_cast<int32_t>(source->shape));
				target.mMetaData->Add(area_spread_key, source->spread);
				if (source->range) target.mMetaData->Add(range_key, *source->range);

				found_.push_back(std::move(light));
			}

			rapidjson::Value const& doc_;
			rapidjson::Value const* lights_{};
			rapidjson::Value const* nodes_{};
			std::vector<std::unique_ptr<aiLight>> found_{};
		};

		// The rules of docs/model-interactions.md marked for import (see Rules
		// there), checked on the whole model: each faulty active object is
		// removed from `result.objects`, with a `dropped` note telling all
		// its problems. The names of the groups are the ones of the nodes of
		// the document, as Assimp may add a node of its own; the names of the
		// lights are the ones of the scene, which are named after their
		// nodes. The problems of the model alone, which leave no object to
		// drop, are only logged.
		void check_imported(interactions::parsed& result, rapidjson::Value const& doc, aiScene const& scene) {
			if (result.objects.empty()) return;

			interactions::model_names model{};
			if (auto const* nodes = member(doc, "nodes"); nodes && nodes->IsArray()) {
				for (auto const& node : nodes->GetArray()) {
					if (auto const* name = member(node, "name"); name && name->IsString())
						model.add_node({name->GetString(), name->GetStringLength()});
				}
			}
			for (unsigned index = 0; index < scene.mNumLights; ++index)
				model.add_light(scene.mLights[index]->mName.C_Str());

			auto const problems = interactions::check(result.objects, model);
			if (problems.empty()) return;

			std::vector<std::string> reasons(result.objects.size());
			for (auto const& [object, reason] : problems) {
				if (!object) {
					warn(std::format("{}: {}", interactions_extra, reason));
					continue;
				}
				auto& joined = reasons[*object];
				if (!joined.empty()) joined.append("; ");
				joined.append(reason);
			}

			gfx::interactions::active_objects kept{};
			kept.reserve(result.objects.size());
			for (size_t index = 0; index < result.objects.size(); ++index) {
				auto& object = result.objects[index];
				if (reasons[index].empty()) {
					kept.push_back(std::move(object));
					continue;
				}
				auto name = std::visit([](auto const& item) { return item.name; }, object);
				result.notes.push_back({.object = std::move(name),
				                        .what = interactions::note::dropped,
				                        .reason = std::move(reasons[index])});
			}
			result.objects = std::move(kept);
		}

		// the PYXEL_interactions extras of every node, in the order of the
		// `nodes` array
		interactions::parsed read_interactions(rapidjson::Value const& doc, aiScene const& scene) {
			interactions::parsed result{};
			auto const* nodes = member(doc, "nodes");
			if (!nodes || !nodes->IsArray()) return result;

			std::vector<std::string> light_names{};
			light_names.reserve(scene.mNumLights);
			for (unsigned index = 0; index < scene.mNumLights; ++index)
				light_names.emplace_back(scene.mLights[index]->mName.C_Str());

			for (rapidjson::SizeType index = 0; index < nodes->Size(); ++index) {
				auto const& node = (*nodes)[index];
				auto const* extras = member(node, "extras");
				auto const* extra = extras ? member(*extras, interactions_extra) : nullptr;
				if (!extra) continue;
				if (!extra->IsArray()) {
					warn(std::format("the {} extra of node {} is not a list; left out", interactions_extra, index));
					continue;
				}
				interactions::parse_into(*extra, light_names, result);
			}

			check_imported(result, doc, scene);

			for (auto const& [object, what, reason] : result.notes) {
				auto const* action = what == interactions::note::dropped   ? "left out"
				                     : what == interactions::note::ignored ? "read in part"
				                                                           : "upgraded";
				warn(std::format("active object `{}` {}: {}", object, action, reason));
			}
			return result;
		}

		// makes InternReadFile() of any importer callable
		struct intern_access : Assimp::BaseImporter {
			using Assimp::BaseImporter::InternReadFile;
		};

		class pyxel_importer final : public Assimp::BaseImporter {
		public:
			explicit pyxel_importer(std::unique_ptr<Assimp::BaseImporter> base) : base_{std::move(base)} {
				desc_ = *base_->GetInfo();
				desc_.mName = importer_name;
			}

			bool CanRead(std::string const& file, Assimp::IOSystem* io, bool check_signature) const override {
				return base_->CanRead(file, io, check_signature);
			}

			aiImporterDesc const* GetInfo() const override { return &desc_; }

			void SetupProperties(Assimp::Importer const* importer) override { base_->SetupProperties(importer); }

			// Assimp does not tell the importer, when the scene is freed, so
			// the address alone could match a later scene of another file,
			// allocated in the same place; the stamp tells them apart
			interactions::parsed const* interactions_of(aiScene const* scene) const noexcept {
				if (!scene || scene != scene_ || !scene->mMetaData) return nullptr;
				uint64_t stamp{};
				if (!scene->mMetaData->Get(import_stamp_key, stamp) || stamp != stamp_) return nullptr;
				return &interactions_;
			}

		protected:
			void InternReadFile(std::string const& file, aiScene* scene, Assimp::IOSystem* io) override {
				scene_ = nullptr;
				interactions_ = {};

				auto const read = &intern_access::InternReadFile;
				((*base_).*read)(file, scene, io);
				// the scene is read, even if no extras can be added to it
				stamp_ = ++last_stamp;
				if (!scene->mMetaData) scene->mMetaData = new aiMetadata{};
				scene->mMetaData->Add(import_stamp_key, stamp_);
				scene_ = scene;

				auto const json = read_json(*io, file);
				if (!json) {
					warn(std::format("cannot read the JSON document of {}; area lights and interactions left out",
					                 file));
					return;
				}
				rapidjson::Document doc{};
				doc.Parse(json->data(), json->size());
				if (doc.HasParseError() || !doc.IsObject()) {
					warn(std::format("cannot parse the JSON document of {}; area lights and interactions left out",
					                 file));
					return;
				}
				area_lights{doc}.add_to(*scene);
				// after the area lights, which add to the names of the lights
				interactions_ = read_interactions(doc, *scene);
			}

		private:
			std::unique_ptr<Assimp::BaseImporter> base_;
			aiImporterDesc desc_{};
			// the scene of the last InternReadFile(), its stamp, and what it
			// found in it
			aiScene const* scene_{};
			uint64_t stamp_{};
			interactions::parsed interactions_{};

			// shared by every importer of ours, so no two scenes get the
			// same stamp
			static inline std::atomic<uint64_t> last_stamp{};
		};
	}  // namespace

	std::string_view exporter_for(std::string_view format_id) noexcept {
		if (format_id == bin_base_format_id) return bin_format_id;
		if (format_id == text_base_format_id) return text_format_id;
		return format_id;
	}

	void register_exporters(Assimp::Exporter& exporter) {
		exporter.RegisterExporter({bin_format_id, "glTF 2.0 (binary), with KHR_lights_punctual and PYXEL_lights_area",
		                           "glb", &export_glb_pyxel});
		exporter.RegisterExporter(
		    {text_format_id, "glTF 2.0, with KHR_lights_punctual and PYXEL_lights_area", "gltf", &export_gltf_pyxel});
	}

	void register_importer(Assimp::Importer& importer) {
		for (size_t index = 0; index < importer.GetImporterCount(); ++index) {
			auto* base = importer.GetImporter(index);
			if (!base || std::string_view{base->GetInfo()->mName} != base_importer_name) continue;

			// unregistered loaders are not deleted by the importer
			importer.UnregisterLoader(base);
			std::unique_ptr<Assimp::BaseImporter> owned{base};
			auto replacement = std::make_unique<pyxel_importer>(std::move(owned));
			importer.RegisterLoader(replacement.release());
			return;
		}
	}

	interactions::parsed const* interactions_of(Assimp::Importer const& importer, aiScene const* scene) {
		for (size_t index = 0; index < importer.GetImporterCount(); ++index) {
			if (auto const* ours = dynamic_cast<pyxel_importer const*>(importer.GetImporter(index)))
				return ours->interactions_of(scene);
		}
		return nullptr;
	}
}  // namespace pyxel::io::gltf2
