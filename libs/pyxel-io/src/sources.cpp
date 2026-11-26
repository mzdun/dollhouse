// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "sources.hpp"
#include <assimp/material.h>
#include <optional>
#include <pyxel/base/str.hpp>
#include <string>
#include <system_error>
#include <utility>

namespace pyxel::io::sources {
	namespace {
		// Assimp builds paths its own way (relative to the working directory,
		// mixed separators, "dir/./file"), and asks for one file more than
		// once
		std::filesystem::path normalized(std::filesystem::path const& path) {
			std::error_code ec{};
			auto result = std::filesystem::weakly_canonical(path, ec);
			if (!ec) return result;
			result = std::filesystem::absolute(path, ec);
			return (ec ? path : result).lexically_normal();
		}

		std::filesystem::path from_u8(std::string_view path) { return std::filesystem::path{as_u8v(path)}; }

		std::optional<unsigned char> hex_digit(char c) noexcept {
			if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
			if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
			if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
			return std::nullopt;
		}
	}  // namespace

	void file_list::add(std::filesystem::path const& path, bool exists) {
		auto file = normalized(path);
		for (auto& known : files_) {
			if (known.path == file) {
				known.exists = known.exists || exists;
				return;
			}
		}
		files_.push_back({.path = std::move(file), .exists = exists});
	}

	void file_list::add_u8(char const* path, bool exists) {
		if (path && *path) add(from_u8(path), exists);
	}

	Assimp::IOStream* recording_io_system::Open(char const* file, char const* mode) {
		auto* const stream = io_.Open(file, mode);
		files_.add_u8(file, stream != nullptr);
		return stream;
	}

	std::string percent_decoded(std::string_view uri) {
		std::string result{};
		result.reserve(uri.size());
		for (size_t index = 0; index < uri.size(); ++index) {
			if (uri[index] == '%' && index + 2 < uri.size()) {
				auto const high = hex_digit(uri[index + 1]);
				auto const low = hex_digit(uri[index + 2]);
				if (high && low) {
					result.push_back(static_cast<char>(*high << 4 | *low));
					index += 2;
					continue;
				}
			}
			result.push_back(uri[index]);
		}
		return result;
	}

	void add_textures(aiScene const& scene, std::filesystem::path const& model_dir, bool uri_paths, file_list& files) {
		for (unsigned material = 0; material < scene.mNumMaterials; ++material) {
			auto const& source = *scene.mMaterials[material];
			for (int type = aiTextureType_NONE; type <= AI_TEXTURE_TYPE_MAX; ++type) {
				auto const texture_type = static_cast<aiTextureType>(type);
				for (unsigned index = 0; index < source.GetTextureCount(texture_type); ++index) {
					aiString name{};
					if (source.GetTexture(texture_type, index, &name) != aiReturn_SUCCESS) continue;

					std::string_view const view{name.C_Str()};
					if (view.empty() || view.starts_with('*') || view.starts_with("data:")) continue;
					if (scene.GetEmbeddedTexture(name.C_Str())) continue;

					auto path = from_u8(uri_paths ? percent_decoded(view) : std::string{view});
					if (path.is_relative()) path = model_dir / path;
					std::error_code ec{};
					auto const exists = std::filesystem::is_regular_file(path, ec);
					files.add(path, exists);
				}
			}
		}
	}
}  // namespace pyxel::io::sources
