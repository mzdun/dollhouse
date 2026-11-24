// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <pyxel/base/str.hpp>
#include <pyxel/io/pose.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "glb.hpp"
#include "gltf2.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

using namespace std::literals;

namespace pyxel::io {
	namespace {
		using allocator = rapidjson::Document::AllocatorType;
		using rapidjson::SizeType;

		std::string text_of(std::filesystem::path const& path) { return as_str(path.generic_u8string()); }

		std::vector<std::byte> read_file(std::filesystem::path const& path) {
			std::ifstream in{path, std::ios::binary};
			if (!in) throw std::runtime_error(std::format("cannot read {}", text_of(path)));
			std::vector<char> chars{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
			if (in.bad()) throw std::runtime_error(std::format("cannot read {}", text_of(path)));
			std::vector<std::byte> result(chars.size());
			std::ranges::transform(chars, result.begin(), [](char c) { return static_cast<std::byte>(c); });
			return result;
		}

		void write_file(std::filesystem::path const& path, std::span<std::byte const> bytes) {
			std::ofstream out{path, std::ios::binary | std::ios::trunc};
			if (out) out.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			if (!out) throw std::runtime_error(std::format("cannot write {}", text_of(path)));
		}

		rapidjson::Document parse(std::string_view json, std::filesystem::path const& source) {
			// a byte order mark, which RapidJSON does not skip
			if (json.starts_with("\xEF\xBB\xBF"sv)) json.remove_prefix(3);

			rapidjson::Document doc{};
			// the numbers kept as they are must not move by a bit
			doc.Parse<rapidjson::kParseFullPrecisionFlag>(json.data(), json.size());
			if (doc.HasParseError()) {
				throw std::runtime_error(std::format("{} is not a glTF 2 file: {}", text_of(source),
				                                     rapidjson::GetParseError_En(doc.GetParseError())));
			}
			if (!doc.IsObject() || !doc.HasMember("asset")) {
				throw std::runtime_error(std::format("{} is not a glTF 2 file", text_of(source)));
			}
			return doc;
		}

		rapidjson::Value* member(rapidjson::Value& parent, char const* name) {
			if (!parent.IsObject()) return nullptr;
			auto const it = parent.FindMember(name);
			return it == parent.MemberEnd() ? nullptr : &it->value;
		}

		std::optional<SizeType> index_of(rapidjson::Value const& value) {
			if (!value.IsUint()) return std::nullopt;
			return value.GetUint();
		}

		void visit(rapidjson::Value& nodes,
		           rapidjson::Value const& list,
		           std::vector<bool>& seen,
		           std::vector<SizeType>& order) {
			for (auto const& item : list.GetArray()) {
				auto const index = index_of(item);
				if (!index || *index >= seen.size() || seen[*index]) continue;
				seen[*index] = true;
				order.push_back(*index);
				if (auto* children = member(nodes[*index], "children"); children && children->IsArray())
					visit(nodes, *children, seen, order);
			}
		}

		// the nodes of the default scene, depth-first, each one once
		std::vector<SizeType> scene_order(rapidjson::Document& doc) {
			std::vector<SizeType> result{};
			auto* nodes = member(doc, "nodes");
			auto* scenes = member(doc, "scenes");
			if (!nodes || !nodes->IsArray() || !scenes || !scenes->IsArray() || scenes->Empty()) return result;

			SizeType scene = 0;
			if (auto* chosen = member(doc, "scene"); chosen && index_of(*chosen)) scene = *index_of(*chosen);
			if (scene >= scenes->Size()) return result;
			auto* roots = member((*scenes)[scene], "nodes");
			if (!roots || !roots->IsArray()) return result;

			std::vector<bool> seen(nodes->Size(), false);
			visit(*nodes, *roots, seen, result);
			return result;
		}

		std::optional<SizeType> find_node(rapidjson::Document& doc,
		                                  std::vector<SizeType> const& order,
		                                  std::string_view name) {
			auto& nodes = doc["nodes"];
			for (auto const index : order) {
				auto* node_name = member(nodes[index], "name");
				if (node_name && node_name->IsString() &&
				    std::string_view{node_name->GetString(), node_name->GetStringLength()} == name)
					return index;
			}
			return std::nullopt;
		}

		template <size_t Size>
		std::optional<std::array<double, Size>> numbers(rapidjson::Value& node, char const* name) {
			auto* array = member(node, name);
			if (!array || !array->IsArray() || array->Size() != Size) return std::nullopt;
			std::array<double, Size> result{};
			for (SizeType index = 0; index < Size; ++index) {
				if (!(*array)[index].IsNumber()) return std::nullopt;
				result[index] = (*array)[index].GetDouble();
			}
			return result;
		}

		// the local matrix of a node: `matrix`, or T · R · S
		glm::dmat4 local_of(rapidjson::Value& node) {
			if (auto const values = numbers<16>(node, "matrix")) {
				glm::dmat4 result{};
				for (glm::length_t column = 0; column < 4; ++column) {
					for (glm::length_t row = 0; row < 4; ++row)
						result[column][row] = (*values)[static_cast<size_t>(column * 4 + row)];
				}
				return result;
			}

			glm::dmat4 result{1.0};
			if (auto const t = numbers<3>(node, "translation"))
				result = glm::translate(result, {(*t)[0], (*t)[1], (*t)[2]});
			// glTF keeps quaternions as x, y, z, w
			if (auto const r = numbers<4>(node, "rotation"))
				result = result * glm::mat4_cast(glm::dquat{(*r)[3], (*r)[0], (*r)[1], (*r)[2]});
			if (auto const s = numbers<3>(node, "scale")) result = glm::scale(result, {(*s)[0], (*s)[1], (*s)[2]});
			return result;
		}

		// rounded to 1e-9, without a negative zero, so the text of the same
		// pose does not depend on the last bits of sin() and cos()
		double rounded(double value) {
			static constexpr double grid = 1e9;
			auto const result = std::round(value * grid) / grid;
			return result == 0.0 ? 0.0 : result;
		}

		void set_matrix(rapidjson::Value& node, glm::dmat4 const& matrix, allocator& alloc) {
			for (auto const* name : {"translation", "rotation", "scale", "matrix"})
				node.RemoveMember(name);

			rapidjson::Value values{rapidjson::kArrayType};
			for (glm::length_t column = 0; column < 4; ++column) {
				for (glm::length_t row = 0; row < 4; ++row)
					values.PushBack(rounded(matrix[column][row]), alloc);
			}
			node.AddMember("matrix", values, alloc);
		}

		// from every node, as the importer reads them from every node
		void drop_interactions(rapidjson::Document& doc) {
			auto* nodes = member(doc, "nodes");
			if (!nodes || !nodes->IsArray()) return;
			for (auto& node : nodes->GetArray()) {
				auto* extras = member(node, "extras");
				if (!extras || !extras->IsObject()) continue;
				extras->RemoveMember(gltf2::interactions_extra);
				if (extras->ObjectEmpty()) node.RemoveMember("extras");
			}
		}

		// "data:...", "https://...", but not "C:/..." or "dir/file.bin"
		bool has_scheme(std::string_view uri) {
			auto const colon = uri.find(':');
			if (colon == std::string_view::npos || colon < 2) return false;
			auto const scheme = uri.substr(0, colon);
			return std::isalpha(static_cast<unsigned char>(scheme.front())) && std::ranges::all_of(scheme, [](char c) {
				       return std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.';
			       });
		}

		void rebase_uris(rapidjson::Document& doc,
		                 std::filesystem::path const& source,
		                 std::filesystem::path const& target) {
			auto const from = std::filesystem::weakly_canonical(std::filesystem::absolute(source).parent_path());
			auto const to = std::filesystem::weakly_canonical(std::filesystem::absolute(target).parent_path());
			if (from == to) return;

			auto& alloc = doc.GetAllocator();
			for (auto const* list_name : {"buffers", "images"}) {
				auto* list = member(doc, list_name);
				if (!list || !list->IsArray()) continue;
				for (auto& item : list->GetArray()) {
					auto* uri = member(item, "uri");
					if (!uri || !uri->IsString()) continue;
					std::string_view const text{uri->GetString(), uri->GetStringLength()};
					std::filesystem::path const relative{as_u8v(text)};
					if (has_scheme(text) || relative.is_absolute() || relative.has_root_name()) continue;

					auto const moved = std::filesystem::weakly_canonical(from / relative).lexically_relative(to);
					if (moved.empty()) {
						throw std::runtime_error(
						    std::format("{} cannot refer to {} with a relative URI", text_of(target), text));
					}
					auto const rebased = as_str(moved.generic_u8string());
					uri->SetString(rebased.data(), static_cast<SizeType>(rebased.size()), alloc);
				}
			}
		}

		std::string lowercase(std::string text) {
			for (auto& c : text)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return text;
		}
	}  // namespace

	void write_posed(std::filesystem::path const& source,
	                 std::filesystem::path const& target,
	                 gfx::loaded_model const& model,
	                 std::span<glm::dmat4 const> moves) {
		auto const bytes = read_file(source);
		auto const binary = glb::is_binary(bytes);

		auto const expected = binary ? ".glb"sv : ".gltf"sv;
		if (lowercase(as_str(target.extension().u8string())) != expected) {
			throw std::runtime_error(std::format("cannot write {}: a copy of a {} glTF 2 file needs the {} extension",
			                                     text_of(target), binary ? "binary"sv : "text"sv, expected));
		}

		std::vector<glb::chunk> chunks{};
		std::string_view json{};
		if (binary) {
			try {
				chunks = glb::read_chunks(bytes);
			} catch (std::runtime_error const& error) {
				throw std::runtime_error(std::format("{} is not a glTF 2 file: {}", text_of(source), error.what()));
			}
			json = {reinterpret_cast<char const*>(chunks.front().data.data()), chunks.front().data.size()};
		} else {
			json = {reinterpret_cast<char const*>(bytes.data()), bytes.size()};
		}

		auto doc = parse(json, source);
		auto& alloc = doc.GetAllocator();

		auto const order = scene_order(doc);
		for (size_t index = 0; index < model.nodes.size() && index < moves.size(); ++index) {
			if (moves[index] == glm::dmat4{1.0}) continue;
			auto const& name = model.nodes[index].name;
			auto const found = name.empty() ? std::nullopt : find_node(doc, order, name);
			if (!found) {
				throw std::runtime_error(std::format("{} has no node `{}` to move", text_of(source), name));
			}
			auto& node = doc["nodes"][*found];
			set_matrix(node, local_of(node) * moves[index], alloc);
		}

		drop_interactions(doc);
		rebase_uris(doc, source, target);

		rapidjson::StringBuffer text{};
		if (binary) {
			rapidjson::Writer<rapidjson::StringBuffer> writer{text};
			doc.Accept(writer);
			auto const out = glb::write({text.GetString(), text.GetSize()}, std::span{chunks}.subspan(1));
			write_file(target, out);
		} else {
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer{text};
			doc.Accept(writer);
			std::string_view const out{text.GetString(), text.GetSize()};
			write_file(target, std::as_bytes(std::span{out}));
		}
	}
}  // namespace pyxel::io
