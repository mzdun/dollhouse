// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/loaded_model.hpp>

namespace verify {
	// Prints the model as an outline on the standard output: the node
	// hierarchy, the lights and every active object, as loaded.
	void print_model(pyxel::gfx::loaded_model const& model);
}  // namespace verify
