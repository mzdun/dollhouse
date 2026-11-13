// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cctype>
#include <charconv>
#include <limits>
#include <pyxel/rng/engine.hpp>
#include <random>

namespace pyxel::rng {
	namespace {
		bool is_valid_seed(std::string_view seed) {
			if (seed.length() != 32) return false;

			for (auto const c : seed) {
				if (!std::isxdigit(static_cast<unsigned char>(c))) {
					return false;
				}
			}

			return true;
		}

		std::string new_seed() {
			std::string result{};
			result.reserve(32);

			using value_type = unsigned char;
			std::random_device rd{};
			std::uniform_int_distribution<unsigned> bits{
			    (std::numeric_limits<value_type>::min)(),
			    (std::numeric_limits<value_type>::max)(),
			};

			for (size_t i = 0; i < 16; ++i) {
				auto const ch = bits(rd);
				static const char hex[] = "0123456789abcdef";
				result += hex[(ch >> 4) & 0xf];
				result += hex[ch & 0xf];
			}

			return result;
		}
	}  // namespace

	engine::engine(std::string const& seed) : seed_{is_valid_seed(seed) ? seed : new_seed()} {}

	engine::engine_t engine::seed_with(std::string_view seed) {
		// the constructor only lets through 32 hex digits
		engine_t::state_type state{};
		for (size_t index = 0; index < state.size(); ++index) {
			auto const digits = seed.substr(index * 8, 8);
			std::from_chars(digits.data(), digits.data() + digits.size(), state[index], 16);
		}
		return engine_t{state};
	}
}  // namespace pyxel::rng
