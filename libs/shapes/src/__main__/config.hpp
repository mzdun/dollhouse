// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace shapes::__main__ {
#define SHAPES_HAS_WATCH 0

	// a script to run: its file and the name it is known by, e.g.
	// `chair` for `chair.model.py`
	struct script {
		std::filesystem::path path{};
		std::string name{};
	};

	struct config {
		std::vector<script> scripts{};
		std::filesystem::path input{};
		std::filesystem::path outdir{};
		std::string exporter_id{};
#if SHAPES_HAS_WATCH
		bool watch{false};
#endif  // SHAPES_HAS_WATCH
		int return_code{-1};

		static config from(std::vector<std::string>& args);
	};
}  // namespace shapes::__main__
