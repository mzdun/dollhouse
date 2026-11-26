// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <memory>
#include <utility>

namespace pyxel {
	// see "Handles" in docs/ui-framework-analysis.md
	template <auto Destroy>
	struct c_deleter {
		template <typename T>
		void operator()(T* ptr) const noexcept {
			Destroy(ptr);
		}
	};

	// an owned C pointer; the deleter is an empty type, so the handle is the
	// size of the pointer
	template <typename T, auto Destroy>
	using c_ptr = std::unique_ptr<T, c_deleter<Destroy>>;

	// a global init with no handle to keep (SDL_Init, ImGui_Impl*_Init):
	// remembers whether it succeeded and calls Shutdown only if it did
	template <auto Shutdown>
	class init_guard {
	public:
		init_guard() = default;
		explicit init_guard(bool ok) noexcept : ok_{ok} {}
		~init_guard() { reset(); }

		init_guard(init_guard&& other) noexcept : ok_{std::exchange(other.ok_, false)} {}
		init_guard& operator=(init_guard&& other) noexcept {
			if (this != &other) {
				reset();
				ok_ = std::exchange(other.ok_, false);
			}
			return *this;
		}
		init_guard(init_guard const&) = delete;
		init_guard& operator=(init_guard const&) = delete;

		explicit operator bool() const noexcept { return ok_; }

		void reset() noexcept {
			if (std::exchange(ok_, false)) Shutdown();
		}

	private:
		bool ok_{false};
	};
}  // namespace pyxel
