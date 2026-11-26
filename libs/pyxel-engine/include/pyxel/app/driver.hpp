// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

namespace pyxel::app {
	// the GPU API asked for first; when it is not there, the backend falls
	// back to the platform's native one (D3D12, Metal)
	enum class gpu_api {
		vulkan,
		native,
	};

	// what the application would like from the driver behind its windows;
	// hints only: a backend takes what it can and ignores the rest, and
	// tells what it ended up with through describe()
	struct driver_hints {
		// wait for the display's refresh; without it, the backend picks the
		// fastest present mode it has
		bool vsync{true};
		gpu_api preferred_api{gpu_api::vulkan};

		bool operator==(driver_hints const&) const noexcept = default;
	};
}  // namespace pyxel::app
