// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <format>
#include <print>
#include <utility>

namespace verify {
	// Prints lines of a YAML-like outline at one depth. The fields of a
	// list item ("- name: ...") and the items under a key ("slots:") go to
	// the outline returned by `nested()`.
	class outline {
	public:
		static constexpr size_t INDENT = 2u;

		constexpr outline() = default;

		template <typename... Args>
		void line(std::format_string<Args...> fmt, Args&&... args) const {
			std::print("{:{}}{}\n", "", depth_ * INDENT, std::format(fmt, std::forward<Args>(args)...));
		}

		constexpr outline nested() const noexcept { return outline{depth_ + 1}; }

	private:
		constexpr explicit outline(size_t depth) noexcept : depth_{depth} {}

		size_t depth_{};
	};
}  // namespace verify
