// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "glb.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace pyxel::io::glb {
	namespace {
		static_assert(std::endian::native == std::endian::little, "GLB is little-endian");

		std::uint32_t read_u32(std::span<std::byte const> bytes, size_t offset) {
			if (offset + 4 > bytes.size()) throw std::runtime_error{"the binary is cut short"};
			std::uint32_t result{};
			std::memcpy(&result, bytes.data() + offset, sizeof(result));
			return result;
		}

		void write_u32(std::vector<std::byte>& out, std::uint32_t value) {
			auto const* bytes = reinterpret_cast<std::byte const*>(&value);
			out.insert(out.end(), bytes, bytes + sizeof(value));
		}

		void write_chunk(std::vector<std::byte>& out, std::uint32_t type, std::span<std::byte const> data) {
			write_u32(out, static_cast<std::uint32_t>(data.size()));
			write_u32(out, type);
			out.insert(out.end(), data.begin(), data.end());
		}
	}  // namespace

	bool is_binary(std::span<std::byte const> bytes) noexcept {
		return bytes.size() >= header_size && read_u32(bytes, 0) == magic && read_u32(bytes, 4) == version;
	}

	std::vector<chunk> read_chunks(std::span<std::byte const> bytes) {
		if (!is_binary(bytes)) throw std::runtime_error{"not a GLB 2 binary"};
		auto const total = std::min<size_t>(read_u32(bytes, 8), bytes.size());

		std::vector<chunk> result{};
		for (size_t offset = header_size; offset + chunk_header_size <= total;) {
			auto const length = read_u32(bytes, offset);
			auto const type = read_u32(bytes, offset + 4);
			offset += chunk_header_size;
			if (offset + length > total) throw std::runtime_error{"a chunk of the binary is cut short"};
			result.push_back({type, bytes.subspan(offset, length)});
			offset += length;
		}
		if (result.empty() || result.front().type != chunk_json)
			throw std::runtime_error{"the binary does not start with JSON"};
		return result;
	}

	std::vector<std::byte> write(std::string_view json, std::span<chunk const> rest) {
		// chunks are aligned to 4 bytes; JSON is padded with spaces
		std::string padded{json};
		while (padded.size() % 4 != 0)
			padded.push_back(' ');

		size_t size = header_size + chunk_header_size + padded.size();
		for (auto const& item : rest)
			size += chunk_header_size + item.data.size();

		std::vector<std::byte> out{};
		out.reserve(size);
		write_u32(out, magic);
		write_u32(out, version);
		write_u32(out, static_cast<std::uint32_t>(size));
		write_chunk(out, chunk_json, std::as_bytes(std::span{padded}));
		for (auto const& [type, data] : rest)
			write_chunk(out, type, data);
		return out;
	}
}  // namespace pyxel::io::glb
