// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "glb_lights.hpp"
#include <assimp/Exceptional.h>
#include <assimp/scene.h>
#include <rapidjson/document.h>
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
#include <vector>

namespace shapes::glb_lights {
	namespace {
		static_assert(std::endian::native == std::endian::little, "GLB is little-endian");

		constexpr auto extension = "KHR_lights_punctual";

		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-gltf-layout
		constexpr std::uint32_t glb_magic = 0x46546C67;  // "glTF"
		constexpr std::uint32_t glb_version = 2;
		constexpr std::uint32_t chunk_json = 0x4E4F534A;  // "JSON"
		constexpr size_t header_size = 12;
		constexpr size_t chunk_header_size = 8;

		[[noreturn]] void fail(std::string const& message) {
			throw DeadlyExportError(std::format("{}: {}", format_id, message));
		}

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
		// at 1. Area (and undefined) lights have no counterpart
		std::optional<rapidjson::Value> make_light(aiLight const& source,
		                                           std::optional<double> range,
		                                           allocator& alloc) {
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

			auto const& color = source.mColorDiffuse;
			auto const intensity = std::max({color.r, color.g, color.b, 0.0f});
			if (intensity > 0.0f) {
				light.AddMember("color",
				                number_array({color.r / intensity, color.g / intensity, color.b / intensity}, alloc),
				                alloc);
			} else {
				light.AddMember("color", number_array({1.0, 1.0, 1.0}, alloc), alloc);
			}
			light.AddMember("intensity", static_cast<double>(intensity), alloc);

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

			if (range && *range > 0.0 && source.mType != aiLightSource_DIRECTIONAL) {
				light.AddMember("range", *range, alloc);
			}
			return light;
		}

		void add_lights(rapidjson::Document& doc, aiScene const& scene) {
			auto& alloc = doc.GetAllocator();
			rapidjson::Value lights{rapidjson::kArrayType};

			auto const nodes_it = doc.FindMember("nodes");
			if (nodes_it != doc.MemberEnd() && nodes_it->value.IsArray()) {
				for (auto& node : nodes_it->value.GetArray()) {
					if (!node.IsObject()) continue;
					auto const extras_it = node.FindMember("extras");
					if (extras_it == node.MemberEnd() || !extras_it->value.IsObject()) continue;
					auto& extras = extras_it->value;

					auto const tag_it = extras.FindMember(light_key);
					if (tag_it == extras.MemberEnd()) continue;
					auto const index = tag_it->value.IsInt() ? tag_it->value.GetInt() : -1;

					std::optional<double> range{};
					if (auto const it = extras.FindMember(range_key);
					    it != extras.MemberEnd() && it->value.IsNumber()) {
						range = it->value.GetDouble();
					}

					// the tags are ours, not of the model
					extras.RemoveMember(light_key);
					extras.RemoveMember(range_key);
					if (extras.ObjectEmpty()) node.RemoveMember("extras");

					if (index < 0 || static_cast<unsigned>(index) >= scene.mNumLights) continue;
					auto light = make_light(*scene.mLights[index], range, alloc);
					if (!light) continue;

					rapidjson::Value ref{rapidjson::kObjectType};
					ref.AddMember("light", lights.Size(), alloc);
					member_object(node, "extensions", alloc).AddMember(rapidjson::StringRef(extension), ref, alloc);
					lights.PushBack(*light, alloc);
				}
			}

			if (lights.Empty()) return;

			rapidjson::Value ext{rapidjson::kObjectType};
			ext.AddMember("lights", lights, alloc);
			member_object(doc, "extensions", alloc).AddMember(rapidjson::StringRef(extension), ext, alloc);

			auto used_it = doc.FindMember("extensionsUsed");
			if (used_it == doc.MemberEnd() || !used_it->value.IsArray()) {
				if (used_it != doc.MemberEnd()) doc.RemoveMember(used_it);
				doc.AddMember("extensionsUsed", rapidjson::Value{rapidjson::kArrayType}, alloc);
				used_it = doc.FindMember("extensionsUsed");
			}
			auto& used = used_it->value;
			auto const listed = std::ranges::any_of(used.GetArray(), [](rapidjson::Value const& item) {
				return item.IsString() && std::string_view{item.GetString()} == extension;
			});
			if (!listed) used.PushBack(rapidjson::StringRef(extension), alloc);
		}

		std::vector<std::byte> rebuild(std::span<std::byte const> blob, aiScene const& scene) {
			auto const chunks = read_chunks(blob);
			auto const& json = chunks.front().data;

			rapidjson::Document doc{};
			doc.Parse(reinterpret_cast<char const*>(json.data()), json.size());
			if (doc.HasParseError() || !doc.IsObject()) fail("the JSON chunk cannot be parsed");

			add_lights(doc, scene);

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
			auto const* blob = base.ExportToBlob(scene, base_format_id, 0u, properties);
			if (!blob) fail(base.GetErrorString());

			auto const bytes = rebuild({static_cast<std::byte const*>(blob->data), blob->size}, *scene);

			auto* stream = io->Open(path, "wb");
			if (!stream) fail(std::format("cannot open {} for writing", path));
			auto const written = stream->Write(bytes.data(), 1, bytes.size());
			io->Close(stream);
			if (written != bytes.size()) fail(std::format("cannot write {}", path));
		}
	}  // namespace

	void register_exporter(Assimp::Exporter& exporter) {
		exporter.RegisterExporter(
		    {format_id, "glTF 2.0 (binary), with KHR_lights_punctual", "glb", &export_glb_lights});
	}
}  // namespace shapes::glb_lights
