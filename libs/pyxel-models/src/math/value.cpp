// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/math/value.hpp>

namespace pyxel::math {
	static_assert(2_mm * 3_mm == area<long long, std::micro>{6});
	static_assert(1_mm == 1'000_um);
	static_assert(1_cm + 1_mm == 11_mm);
}  // namespace pyxel::math
