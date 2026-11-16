// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/sink.hpp>

namespace pyxel::gfx {
	class object {
	public:
		object() = default;
		object(object const&) = delete;
		object(object&&) = delete;
		object& operator=(object const&) = delete;
		object& operator=(object&&) = delete;

		virtual ~object();
		// `parent` is relative to the current node of the sink (the nearest
		// named group), not to the world; it is only meant to be given to
		// sink::add_solid or to the children's output
		virtual void output(sink&, glm::dmat4 const& parent = {1.0}) const = 0;
	};
}  // namespace pyxel::gfx
