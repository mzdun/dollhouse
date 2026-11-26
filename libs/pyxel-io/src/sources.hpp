// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <assimp/DefaultIOSystem.h>
#include <assimp/scene.h>
#include <filesystem>
#include <pyxel/io/import.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pyxel::io::sources {
	// the files of one import, each one once, in the order they were added
	class file_list {
	public:
		// a file found once stays found
		void add(std::filesystem::path const& path, bool exists);
		// with the path in UTF-8, as Assimp has it
		void add_u8(char const* path, bool exists);

		std::vector<source_file> take() && noexcept { return std::move(files_); }

	private:
		std::vector<source_file> files_{};
	};

	// The default file system of Assimp, telling `files` about each file
	// opened, or not found when opened. Assimp's importers read every file
	// through it, so this is the whole list, except for the textures, which
	// they only name. A file only looked for is not on the list: when an
	// open fails, Assimp's FileSystemFilter looks for the file in made-up
	// places (the directory of the model with pieces of the path cut
	// off), and the file the importer wanted is on the list already.
	class recording_io_system final : public Assimp::IOSystem {
	public:
		explicit recording_io_system(file_list& files) : files_{files} {}

		bool Exists(char const* file) const override { return io_.Exists(file); }
		char getOsSeparator() const override { return io_.getOsSeparator(); }
		Assimp::IOStream* Open(char const* file, char const* mode) override;
		void Close(Assimp::IOStream* file) override { io_.Close(file); }
		bool ComparePaths(char const* one, char const* second) const override { return io_.ComparePaths(one, second); }

	private:
		Assimp::DefaultIOSystem io_{};
		file_list& files_;
	};

	// "%20" and the like turned back into the bytes they stand for; an
	// escape that is cut short or not hexadecimal stays as it is
	std::string percent_decoded(std::string_view uri);

	// The external textures of the materials, against `model_dir`. Skips
	// the embedded ones ("*0" or a name of a texture in the scene) and the
	// data URIs. The glTF 2 textures are URIs, so they are percent-decoded
	// first, when `uri_paths` is set.
	void add_textures(aiScene const& scene, std::filesystem::path const& model_dir, bool uri_paths, file_list& files);
}  // namespace pyxel::io::sources
