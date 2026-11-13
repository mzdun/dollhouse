// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace shapes::__main__ {
#define SHAPES_HAS_WATCH 0

	struct config {
		std::vector<std::string> modules{};
		std::filesystem::path input{};
		std::filesystem::path outdir{};
		std::string exporter_id{};
#if SHAPES_HAS_WATCH
		bool watch{false};
#endif  // SHAPES_HAS_WATCH
		bool modules_in_args{false};
		int return_code{-1};

		static config from(std::vector<std::string>& args);
	};
}  // namespace shapes::__main__
