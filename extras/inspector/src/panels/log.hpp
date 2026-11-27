// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>

#include "moving_average.hpp"

namespace inspector {
	class application;
}  // namespace inspector

namespace inspector::panels {
	// the Log window: the version, the frame rate, the state of the loader
	// and what the last import had to say
	struct log {
		std::string header{};
		moving_average<double, 10000> fps{};

		void draw(application const& app);
	};
}  // namespace inspector::panels
