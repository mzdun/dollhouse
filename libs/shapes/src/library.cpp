// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <format>
#include <shapes/library.hpp>
#include <stdexcept>
#include <utility>
#include "export.hpp"

namespace shapes {
	namespace {
		library* current = nullptr;
	}  // namespace

	library& library::python_instance() {
		// there is always a library alive while scripts run
		return *current;
	}

	library::library() : previous_{current} { current = this; }

	library::~library() {
		if (current == this) current = previous_;
	}

	bool library::add_model(std::string name, model_fn fn) {
		return models_.try_emplace(std::move(name), std::move(fn)).second;
	}
}  // namespace shapes
