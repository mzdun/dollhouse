// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "glb_lights.hpp"
#include <assimp/Exceptional.h>
#include <assimp/scene.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pyxel::io::glb_lights {
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

		// PYXEL_lights_area (see glb_lights.hpp)
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

		// the JSON document from Assimp, with the lights added
		rapidjson::Document parse_with_lights(char const* exporter, std::string_view json, aiScene const& scene) {
			rapidjson::Document doc{};
			doc.Parse(json.data(), json.size());
			if (doc.HasParseError() || !doc.IsObject()) fail(exporter, "the JSON document cannot be parsed");

			add_lights(doc, scene);
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

		std::vector<std::byte> rebuild(std::span<std::byte const> blob, aiScene const& scene) {
			auto const chunks = read_chunks(blob);
			auto const& json = chunks.front().data;
			auto const doc =
			    parse_with_lights(bin_format_id, {reinterpret_cast<char const*>(json.data()), json.size()}, scene);

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

		void export_glb_lights(char const* path,
		                       Assimp::IOSystem* io,
		                       aiScene const* scene,
		                       Assimp::ExportProperties const* properties) {
			// the scene was post-processed already, by the exporter calling
			// this function
			Assimp::Exporter base{};
			auto const* blob = base.ExportToBlob(scene, bin_base_format_id, 0u, properties);
			if (!blob) fail(base.GetErrorString());

			auto const bytes = rebuild({static_cast<std::byte const*>(blob->data), blob->size}, *scene);
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

		void export_gltf_lights(char const* path,
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

			auto const doc = parse_with_lights(text_format_id, json, *scene);

			// indented, as Assimp writes it
			rapidjson::StringBuffer text{};
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{text};
			doc.Accept(writer);
			write_file(text_format_id, *io, path, "wt", text.GetString(), text.GetSize());
		}
	}  // namespace

	std::string_view exporter_for(std::string_view format_id) noexcept {
		if (format_id == bin_base_format_id) return bin_format_id;
		if (format_id == text_base_format_id) return text_format_id;
		return format_id;
	}

	void register_exporters(Assimp::Exporter& exporter) {
		exporter.RegisterExporter({bin_format_id, "glTF 2.0 (binary), with KHR_lights_punctual and PYXEL_lights_area",
		                           "glb", &export_glb_lights});
		exporter.RegisterExporter(
		    {text_format_id, "glTF 2.0, with KHR_lights_punctual and PYXEL_lights_area", "gltf", &export_gltf_lights});
	}
}  // namespace pyxel::io::glb_lights
