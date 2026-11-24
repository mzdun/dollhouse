// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/io/import.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace verify {
	enum class severity { info, warning, error };

	struct diagnostic {
		verify::severity severity;
		std::string object;  // the active object, if the problem is in one
		std::string message;
	};

	// What the importer said about the active objects, followed by what it
	// leaves to the viewer: states the object can never reach, actions it
	// can never offer, and groups or lights the model does not have.
	std::vector<diagnostic> check(pyxel::io::imported_model const& imported);

	std::string_view name_of(severity level) noexcept;
}  // namespace verify
