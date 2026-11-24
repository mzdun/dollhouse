// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// The binary container of glTF 2: a header, then chunks, the first of them
// JSON (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#binary-gltf-layout).
namespace pyxel::io::glb {
	inline constexpr std::uint32_t magic = 0x46546C67;  // "glTF"
	inline constexpr std::uint32_t version = 2;
	inline constexpr std::uint32_t chunk_json = 0x4E4F534A;  // "JSON"
	inline constexpr size_t header_size = 12;
	inline constexpr size_t chunk_header_size = 8;

	struct chunk {
		std::uint32_t type;
		std::span<std::byte const> data;
	};

	// true, if `bytes` start with the header of a GLB 2 binary
	bool is_binary(std::span<std::byte const> bytes) noexcept;

	// The chunks of a GLB 2 binary, viewing `bytes`; throws
	// std::runtime_error, if it is not one, if a chunk is cut short, or if
	// the first chunk is not JSON.
	std::vector<chunk> read_chunks(std::span<std::byte const> bytes);

	// A GLB 2 binary with `json` as the first chunk, padded with spaces,
	// followed by `rest`, as they are.
	std::vector<std::byte> write(std::string_view json, std::span<chunk const> rest);
}  // namespace pyxel::io::glb
