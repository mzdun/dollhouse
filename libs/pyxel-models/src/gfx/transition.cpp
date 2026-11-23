// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <pyxel/gfx/transition.hpp>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

using namespace std::literals;

namespace pyxel::gfx::interactions {
	namespace {
		constexpr std::array channels{
		    "all"sv,      "translate"sv, "translate.x"sv, "translate.y"sv, "translate.z"sv, "rotate"sv,  "rotate.x"sv,
		    "rotate.y"sv, "rotate.z"sv,  "scale"sv,       "scale.x"sv,     "scale.y"sv,     "scale.z"sv, "set"sv,
		};

		constexpr bool is_space(char c) noexcept {
			return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
		}

		constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

		std::string_view trim(std::string_view text) {
			while (!text.empty() && is_space(text.front()))
				text.remove_prefix(1);
			while (!text.empty() && is_space(text.back()))
				text.remove_suffix(1);
			return text;
		}

		std::string lowercase(std::string_view text) {
			std::string result{text};
			for (auto& c : result) {
				if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
			}
			return result;
		}

		std::optional<easing> easing_of(std::string_view token) {
			if (token == "ease"sv) return easing::ease;
			if (token == "ease-in"sv) return easing::ease_in;
			if (token == "ease-out"sv) return easing::ease_out;
			if (token == "ease-in-out"sv) return easing::ease_in_out;
			if (token == "linear"sv) return easing::linear;
			return std::nullopt;
		}

		bool is_channel(std::string_view token) { return std::ranges::find(channels, token) != channels.end(); }

		// a <time> of CSS: a <number> with the unit `s` or `ms`, in seconds
		std::optional<double> time_of(std::string_view token) {
			size_t pos = 0;
			auto const digits = [&] {
				auto const start = pos;
				while (pos < token.size() && is_digit(token[pos]))
					++pos;
				return pos > start;
			};

			if (pos < token.size() && (token[pos] == '+' || token[pos] == '-')) ++pos;
			auto const integral = digits();
			auto fraction = false;
			if (pos < token.size() && token[pos] == '.') {
				++pos;
				fraction = digits();
				if (!fraction) return std::nullopt;
			}
			if (!integral && !fraction) return std::nullopt;

			// an exponent only, if digits follow; otherwise the `e` is not
			// a part of the number
			if (pos < token.size() && token[pos] == 'e') {
				auto next = pos + 1;
				if (next < token.size() && (token[next] == '+' || token[next] == '-')) ++next;
				if (next < token.size() && is_digit(token[next])) {
					pos = next;
					digits();
				}
			}

			auto const number = token.substr(0, pos);
			auto const unit = token.substr(pos);
			double scale{};
			if (unit == "s"sv) {
				scale = 1.0;
			} else if (unit == "ms"sv) {
				scale = 0.001;
			} else {
				return std::nullopt;
			}

			// from_chars reads every form accepted above, but the leading `+`;
			// unlike strtod, it does not depend on the locale
			auto digits_only = number;
			if (digits_only.starts_with('+')) digits_only.remove_prefix(1);
			double parsed{};
			auto const* const end = digits_only.data() + digits_only.size();
			auto const [ptr, ec] = std::from_chars(digits_only.data(), end, parsed);
			if (ec != std::errc{} || ptr != end) return std::nullopt;

			auto const value = parsed * scale;
			if (!std::isfinite(value)) return std::nullopt;
			return std::max(value, 0.0);
		}

		std::optional<single_transition> parse_single(std::string_view item, std::string& error) {
			single_transition result{};
			bool has_channel = false;
			bool has_easing = false;
			size_t times = 0;

			while (!item.empty()) {
				auto const end = static_cast<size_t>(std::ranges::find_if(item, is_space) - item.begin());
				auto const token = item.substr(0, end);
				item = trim(item.substr(end));

				if (is_channel(token)) {
					if (has_channel) {
						error = std::format("names two channels, `{}` and `{}`", result.channel, token);
						return std::nullopt;
					}
					has_channel = true;
					result.channel = token;
				} else if (auto const timing = easing_of(token)) {
					if (has_easing) {
						error = std::format("has two timing functions, including `{}`", token);
						return std::nullopt;
					}
					has_easing = true;
					result.easing = *timing;
				} else if (auto const time = time_of(token)) {
					if (times == 2) {
						error = std::format("has more than two times, including `{}`", token);
						return std::nullopt;
					}
					(times == 0 ? result.duration : result.delay) = *time;
					++times;
				} else {
					error = std::format("has `{}`, which is not a channel, a timing function or a time", token);
					return std::nullopt;
				}
			}
			return result;
		}
	}  // namespace

	transition_result parse_transition(std::string_view value) {
		auto const text = lowercase(trim(value));
		if (text == "none"sv) return {};

		transition_result result{};
		std::string_view rest{text};
		while (true) {
			auto const comma = rest.find(',');
			auto const item = trim(rest.substr(0, comma));
			if (item.empty()) {
				result.items.clear();
				result.error = "has an empty item"s;
				return result;
			}
			auto single = parse_single(item, result.error);
			if (!single) {
				result.items.clear();
				return result;
			}
			result.items.push_back(std::move(*single));
			if (comma == std::string_view::npos) break;
			rest = rest.substr(comma + 1);
		}
		return result;
	}
}  // namespace pyxel::gfx::interactions
