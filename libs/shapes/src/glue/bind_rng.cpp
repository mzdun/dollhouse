// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cstdint>
#include <format>
#include <pyxel/rng/engine.hpp>
#include <shapes/glue/glue.hpp>
#include <string>

namespace shapes::glue {
	namespace {
		using pyxel::rng::dice;
		using pyxel::rng::engine;
		using pyxel::rng::spinner;

		void print_seed(engine const& rng) { py::print("-- seed:", rng.seed()); }

		engine init_seedless() {
			auto result = engine{};
			print_seed(result);
			return result;
		}

		engine init_seeded(std::string const& seed) {
			auto result = engine{seed};
			if (seed != result.seed()) print_seed(result);
			return result;
		}

		dice sided_dice(engine& self, std::intmax_t sides) {
			if (sides < 1)
				throw py::value_error(std::format("RNG.dice: a dice needs at least one side, got {}", sides));
			return self.dice(sides);
		}
	}  // namespace

	void bind_rng(py::module_& m) {
		py::class_<dice>(m, "dice",
		                 "Uniform integer dice, made by `RNG.dice`. Rolls the same numbers for a given seed on every "
		                 "platform, and keeps its `RNG` alive.")
		    .def("roll", &dice::roll, "Rolls the dice: a number from `min` to `max`, both included.")
		    .def_property_readonly("min", &dice::min, "Lowest possible roll.")
		    .def_property_readonly("max", &dice::max, "Highest possible roll.");

		py::class_<spinner>(m, "spinner",
		                    "Uniform real dice, made by `RNG.spinner`. Spins the same numbers for a given seed on "
		                    "every platform, and keeps its `RNG` alive.")
		    .def("spin", &spinner::spin, "Spins the spinner: a number from `min` (included) to `max` (excluded).")
		    .def_property_readonly("min", &spinner::min, "Lowest possible spin.")
		    .def_property_readonly("max", &spinner::max, "End of the range; spins stay under it.");

		py::class_<engine>(m, "RNG",
		                   "Seeded random number generator (xoshiro128**). The same seed always rolls the same "
		                   "numbers, so a printed seed reproduces a model.")
		    .def(py::init(&init_seedless), "Creates a generator with a new random seed, and prints that seed.")
		    .def(py::init(&init_seeded), "seed"_a,
		         "Creates a generator from a seed of 32 hex digits. Any other seed is replaced with a new random "
		         "one, which is printed.")
		    .def_property_readonly("seed", &engine::seed, "Seed in use: 32 hex digits.")
		    .def("dice", &sided_dice, "sides"_a, py::keep_alive<0, 1>(),
		         "Returns a dice rolling 1 to `sides`; raises ValueError for fewer than one side.")
		    .def("dice", py::overload_cast<std::intmax_t, std::intmax_t>(&engine::dice), "min"_a, "max"_a,
		         py::keep_alive<0, 1>(),
		         "Returns a dice rolling `min` to `max`, both included; the ends are swapped, "
		         "if `min` is greater than `max`.")
		    .def(
		        "spinner", [](engine& self, double min, double max) { return self.spinner(min, max); }, "min"_a = 0.0,
		        "max"_a = 1.0, py::keep_alive<0, 1>(),
		        "Returns a spinner landing from `min` (included) to `max` (excluded), with the full precision of a "
		        "float; the ends are swapped, if `min` is greater than `max`.");
	}
}  // namespace shapes::glue
