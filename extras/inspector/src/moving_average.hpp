// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <array>
#include <cstddef>

namespace inspector {
	template <typename T, std::size_t N>
	struct moving_average {
		static_assert(N > 2);
		std::array<T, N> values{};
		T running_sum{};
		std::size_t used{0};
		std::size_t index{0};

		T observe(T value) {
			if (index == N) {
				index = 0;
			}
			running_sum -= values[index];
			values[index] = value;
			++index;
			running_sum += value;

			if (used < N) {
				++used;
			}

			return running_sum / static_cast<T>(used);
		}
	};
}  // namespace inspector
