// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/object.hpp>
#include <pyxel/gfx/space.hpp>

namespace pyxel::gfx {
	// base of the objects defined by their bounding box; output() is left to
	// the subclasses. The modifiers return the object itself, typed as the
	// subclass it was called on, so the calls can be chained. The size is not
	// checked; it is up to the subclass to decide what a negative edge means
	struct sized : object {
		sized(gfx::size const& size) : size_{size} {}
		sized(length width, length height, length depth) : size_{width, height, depth} {}

		gfx::size const& size() const noexcept { return size_; }

		template <typename Self>
		Self& set_size(this Self& self, gfx::size const& size) noexcept {
			self.size_ = size;
			return self;
		}
		template <typename Self>
		Self& set_size(this Self& self, length width, length height, length depth) noexcept {
			return self.set_size(gfx::size{width, height, depth});
		}

		// replaces the given edges, keeping the others
		template <typename Self>
		Self& resize(this Self& self, gfx::size::changes const& diff) noexcept {
			return self.set_size(self.size_.replace(diff));
		}

		// adds `delta` to the edges
		template <typename Self>
		Self& grow_by(this Self& self, gfx::size const& delta) noexcept {
			return self.set_size(self.size_ + delta);
		}
		template <typename Self>
		Self& grow_by(this Self& self, length dwidth, length dheight, length ddepth) noexcept {
			return self.grow_by(gfx::size{dwidth, dheight, ddepth});
		}

		// multiplies all edges by `factor`
		template <typename Self>
		Self& scale(this Self& self, double factor) noexcept {
			return self.set_size(self.size_ * factor);
		}
		// multiplies each edge by its own factor; the empty ones are kept
		template <typename Self>
		Self& scale(this Self& self, gfx::size::factors const& by) noexcept {
			return self.set_size(self.size_.scaled(by));
		}

	private:
		gfx::size size_{};
	};
}  // namespace pyxel::gfx
