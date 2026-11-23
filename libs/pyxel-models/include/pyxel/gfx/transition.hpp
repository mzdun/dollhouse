// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <string_view>
#include <vector>

// The transition values of active objects (see Transitions grammar in
// docs/model-interactions.md), read from the text the declarations keep.
namespace pyxel::gfx::interactions {
	enum class easing { ease, ease_in, ease_out, ease_in_out, linear };

	// How the change of one channel unfolds; the default is `none`, that is
	// `0s ease 0s`.
	struct timing {
		interactions::easing easing{easing::ease};
		double duration{};  // seconds
		double delay{};     // seconds

		bool operator==(timing const&) const = default;
	};

	// One item of a transition value, with the omitted parts filled from
	// `all 0s ease 0s`; a negative time is taken as 0s.
	struct single_transition {
		std::string channel{"all"};
		interactions::easing easing{easing::ease};
		double duration{};  // seconds
		double delay{};     // seconds

		interactions::timing timing() const noexcept {
			return {.easing = easing, .duration = duration, .delay = delay};
		}

		bool operator==(single_transition const&) const = default;
	};

	struct transition_result {
		// empty for `none`
		std::vector<single_transition> items{};
		// empty, if the value parses
		std::string error{};
	};

	// The grammar of Transitions grammar in the specification; keywords and
	// units are ASCII case-insensitive, the channel names come back in
	// lowercase. The channels are not matched with any target here.
	transition_result parse_transition(std::string_view value);
}  // namespace pyxel::gfx::interactions
