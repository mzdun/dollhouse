// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <rapidjson/document.h>
#include <pyxel/io/interactions.hpp>
#include <span>
#include <string>

namespace pyxel::io::interactions {
	// parse() on a document read already, as the importer has it: reads
	// every item of `list`, a JSON array, and appends the objects and the
	// notes to `out`
	void parse_into(rapidjson::Value const& list, std::span<std::string const> light_names, parsed& out);
}  // namespace pyxel::io::interactions
