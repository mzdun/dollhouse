// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <args/parser.hpp>
#include <filesystem>
#include <optional>
#include <pyxel/app/driver.hpp>

namespace inspector {
	struct options {
		// absolute and normalized; an existing regular file
		std::optional<std::filesystem::path> model_path{};
		pyxel::app::driver_hints driver{};
		bool verbose{false};
	};

	// Reads the command line. Ends the process on --help, on --version and
	// on a model path, which is not an existing file.
	options parse_options(::args::args_view const& args);
}  // namespace inspector
