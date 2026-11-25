// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cstddef>
#include <optional>
#include <pyxel/gfx/loaded_model.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// The actions given with --emulate, taken one after another on the active
// objects, each of them starting in its starting state. A state change is
// taken as finished at once: there are no transitions and no wires.
namespace verify {
	// the action standing for the default interaction, as the outline of
	// the model shows it
	inline constexpr std::string_view click = "<click>";

	struct emulated_action {
		std::string action;
		std::string object;

		bool operator==(emulated_action const&) const = default;
	};

	// A problem with one of the --emulate values: it does not parse, or it
	// names an object, or an action, the model does not have.
	class emulation_error : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};

	// Splits `<action> <object>`. The object is the last chunk: the text in
	// the last pair of double quotes, if the value ends with a quote, or
	// the last word otherwise. The rest is the action, without the quotes
	// around it, if it has them. So `Turn on "left rocker"` is "Turn on" on
	// "left rocker", and `Turn on left rocker` is "Turn on left" on "rocker".
	// Throws emulation_error, if either of them is missing, or if a quote
	// is not closed.
	emulated_action parse_emulated(std::string_view value);

	struct emulated_step {
		std::string object;
		std::string action;  // as in the outline: "\"Open\"" or "<click>"
		std::string from;
		std::string to;
	};

	class emulator {
	public:
		explicit emulator(pyxel::gfx::loaded_model const& model);

		// Takes the action on the first active object of the name, if the
		// current state of the object offers it; throws emulation_error
		// otherwise, leaving the states as they were.
		emulated_step take(emulated_action const& request);

		// one per active object of the model, for loaded_model::moves()
		std::span<std::optional<size_t> const> states() const noexcept { return states_; }

	private:
		pyxel::gfx::loaded_model const& model_;
		std::vector<std::optional<size_t>> states_{};
	};
}  // namespace verify
