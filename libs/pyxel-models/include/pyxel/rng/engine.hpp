// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/rng/dice.hpp>
#include <pyxel/rng/spinner.hpp>
#include <pyxel/rng/xoshiro.hpp>
#include <string>
#include <string_view>

namespace pyxel::rng {
	class engine {
	public:
		using engine_t = xoshiro128starstar;
		using result_type = engine_t::result_type;

		// a seed is 32 hex digits, the four 32-bit words of the xoshiro
		// state, most significant digit first; any other seed is replaced
		// with a new random one
		explicit engine(std::string const& seed = {});

		auto operator()() noexcept(noexcept(engine_())) { return engine_(); }
		static constexpr auto min() noexcept(noexcept(engine_.min())) { return engine_t::min(); }
		static constexpr auto max() noexcept(noexcept(engine_.max())) { return engine_t::max(); }

		rng::dice dice(std::intmax_t sides) noexcept { return dice(1, sides); }
		rng::dice dice(std::intmax_t min, std::intmax_t max) noexcept { return rng::dice{engine_, min, max}; }

		template <std::floating_point Float>
		basic_spinner<Float> spinner(Float min, Float max) noexcept {
			return basic_spinner<Float>{engine_, min, max};
		}

		std::string const& seed() const noexcept { return seed_; }

	private:
		static engine_t seed_with(std::string_view seed);

		std::string seed_{};
		engine_t engine_{seed_with(seed_)};
	};
}  // namespace pyxel::rng
