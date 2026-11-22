// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "interactions_rules.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

using namespace std::literals;

namespace pyxel::io::interactions {
	namespace {
		namespace decl = gfx::interactions;

		constexpr char slot_prefix = '$';
		constexpr auto reserved_slot = "visible"sv;

		constexpr std::array group_channels{
		    "all"sv,      "translate"sv, "translate.x"sv, "translate.y"sv, "translate.z"sv, "rotate"sv,  "rotate.x"sv,
		    "rotate.y"sv, "rotate.z"sv,  "scale"sv,       "scale.x"sv,     "scale.y"sv,     "scale.z"sv,
		};
		constexpr std::array value_channels{"all"sv, "set"sv};

		// ---- transitions ---------------------------------------------------

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

		bool is_channel(std::string_view token) {
			return token == "set"sv || std::ranges::find(group_channels, token) != group_channels.end();
		}

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

		// ---- checks --------------------------------------------------------

		class object_checker {
		public:
			object_checker(model_names const& model, std::vector<problem>& out, size_t index)
			    : model_{model}, out_{out}, index_{index} {}

			void check(decl::light_active_object const& item) {
				check_name(item.name);
				if (!model_.lights.contains(item.name)) report("describes no light of the model"s);
				if (!item.light.empty()) check_rule("the transition of the light"s, item.light, value_channels);
			}

			void check(decl::geo_active_object const& item) {
				check_name(item.name);
				if (model_.lights.contains(item.name))
					report("takes the name of a light, but is not an object describing it"s);

				check_slots(item.slots);
				check_states(item.states);
				check_actions(item.actions, item.states);
				check_transitions(item.transitions);
			}

		private:
			void report(std::string reason) {
				// the same group or slot may be named in many states
				if (std::ranges::find(reported_, reason) != reported_.end()) return;
				reported_.push_back(reason);
				out_.push_back({.object = index_, .reason = std::move(reason)});
			}

			void check_name(std::string const& name) {
				if (name.empty()) report("has no name"s);
				if (name.find_first_of("./"sv) != std::string::npos)
					report(std::format("has a name with a `.` or a `/` in it"));
			}

			// ---- slots ---------------------------------------------------------

			static bool type_matches(decl::slot_value const& value, decl::slot_type type) {
				switch (type) {
					case decl::slot_type::boolean:
						return std::holds_alternative<bool>(value);
					case decl::slot_type::floating:
					case decl::slot_type::factor:
						return std::holds_alternative<double>(value);
					case decl::slot_type::angle:
						return std::holds_alternative<gfx::angle>(value);
					case decl::slot_type::length:
						return std::holds_alternative<gfx::length>(value);
					case decl::slot_type::offset:
						return std::holds_alternative<gfx::offset>(value);
				}
				return false;
			}

			static bool finite(decl::slot_value const& value) {
				return std::visit(
				    [](auto const& item) {
					    using T = std::remove_cvref_t<decltype(item)>;
					    if constexpr (std::is_same_v<T, bool>) {
						    return true;
					    } else if constexpr (std::is_same_v<T, double>) {
						    return std::isfinite(item);
					    } else if constexpr (std::is_same_v<T, gfx::offset>) {
						    return std::isfinite(item.x.count()) && std::isfinite(item.y.count()) &&
						           std::isfinite(item.z.count());
					    } else {
						    return std::isfinite(item.count());
					    }
				    },
				    value);
			}

			// the value is one the type of the slot accepts
			static bool accepts(decl::slot const& slot, decl::slot_value const& value) {
				if (!type_matches(value, slot.type) || !finite(value)) return false;
				if (slot.type != decl::slot_type::factor) return true;
				auto const factor = std::get<double>(value);
				return factor >= 0.0 && factor <= 1.0;
			}

			void check_slots(std::vector<decl::slot> const& slots) {
				std::set<std::string_view> seen{};
				for (auto const& slot : slots) {
					auto const what = std::format("slot `{}`", slot.name);
					if (slot.name.empty()) report("has a slot with no name"s);
					if (slot.name.find_first_of("./"sv) != std::string::npos)
						report(std::format("has {}, with a `.` or a `/` in its name", what));
					if (slot.name == reserved_slot) report(std::format("has {}, which is reserved", what));
					if (!seen.insert(slot.name).second) report(std::format("has {} declared twice", what));
					if (slot.default_value && !accepts(slot, *slot.default_value))
						report(std::format("has {} with a default, which its type does not accept", what));
				}
				slots_ = &slots;
			}

			decl::slot const* find_slot(std::string_view name) const {
				auto const it = std::ranges::find(*slots_, name, &decl::slot::name);
				return it == slots_->end() ? nullptr : &*it;
			}

			// the slot, if it is one a state or a transition may change
			decl::slot const* changeable_slot(std::string_view name, std::string_view where) {
				auto const* slot = find_slot(name);
				if (!slot) {
					report(std::format("names slot `{}` {}, which it does not declare", name, where));
					return nullptr;
				}
				if (slot->direction == decl::direction::input) {
					report(std::format("names slot `{}` {}, which is an input", name, where));
					return nullptr;
				}
				return slot;
			}

			// ---- groups --------------------------------------------------------

			void check_group(std::string_view group, std::string_view where) {
				if (group.starts_with(slot_prefix)) {
					report(std::format("names group `{}` {}, which starts with a `$`", group, where));
					return;
				}
				if (model_.lights.contains(group)) {
					report(std::format("names group `{}` {}, which is the name of a light", group, where));
					return;
				}
				auto const it = model_.nodes.find(group);
				auto const count = it == model_.nodes.end() ? size_t{0} : it->second;
				if (count == 0) {
					report(std::format("names group `{}` {}, which is not a node of the model", group, where));
				} else if (count > 1) {
					report(std::format("names group `{}` {}, which is the name of {} nodes", group, where, count));
				}
			}

			// a channel of a group, as named by the transitions
			static std::string_view channel_of(decl::group_transformation const& change) {
				static constexpr std::array translate{"translate.x"sv, "translate.y"sv, "translate.z"sv};
				static constexpr std::array rotate{"rotate.x"sv, "rotate.y"sv, "rotate.z"sv};
				static constexpr std::array scale{"scale.x"sv, "scale.y"sv, "scale.z"sv};
				struct visitor {
					std::string_view operator()(decl::translate_axis const& item) const {
						return translate[std::to_underlying(item.on)];
					}
					std::string_view operator()(decl::translate_all const&) const { return "translate"sv; }
					std::string_view operator()(decl::rotate_axis const& item) const {
						return rotate[std::to_underlying(item.around)];
					}
					std::string_view operator()(decl::rotate_all const&) const { return "rotate"sv; }
					std::string_view operator()(decl::scale_axis const& item) const {
						return scale[std::to_underlying(item.along)];
					}
					std::string_view operator()(decl::scale_all const&) const { return "scale"sv; }
				};
				return std::visit(visitor{}, change);
			}

			static bool finite(decl::group_transformation const& change) {
				struct visitor {
					bool operator()(decl::translate_axis const& item) const { return std::isfinite(item.by.count()); }
					bool operator()(decl::translate_all const& item) const {
						return std::isfinite(item.by.x.count()) && std::isfinite(item.by.y.count()) &&
						       std::isfinite(item.by.z.count());
					}
					bool operator()(decl::rotate_axis const& item) const { return std::isfinite(item.by.count()); }
					bool operator()(decl::rotate_all const& item) const {
						return std::isfinite(item.x.count()) && std::isfinite(item.y.count()) &&
						       std::isfinite(item.z.count());
					}
					bool operator()(decl::scale_axis const& item) const { return std::isfinite(item.by); }
					bool operator()(decl::scale_all const& item) const {
						return std::isfinite(item.x) && std::isfinite(item.y) && std::isfinite(item.z);
					}
				};
				return std::visit(visitor{}, change);
			}

			// ---- states --------------------------------------------------------

			void check_states(decl::states const& states) {
				std::set<std::string_view> seen{};
				// the order of the channels of each group, as they first
				// appear in the states
				std::map<std::string_view, std::vector<std::string_view>> orders{};

				for (auto const& state : states) {
					if (!seen.insert(state.name).second) report(std::format("declares state `{}` twice", state.name));

					for (auto const& [group, changes] : state.groups) {
						auto const where = std::format("in state `{}`", state.name);
						check_group(group, where);

						auto& order = orders[group];
						std::set<std::string_view> listed{};
						for (auto const& change : changes) {
							auto const channel = channel_of(change);
							if (!listed.insert(channel).second) {
								report(std::format("sets `{}` of group `{}` {} twice", channel, group, where));
							}
							if (!finite(change)) {
								report(std::format("sets `{}` of group `{}` {} to a value, which is not finite",
								                   channel, group, where));
							}
							if (std::ranges::find(order, channel) == order.end()) order.push_back(channel);
						}
					}

					for (auto const& [name, changes] : state.slots) {
						auto const where = std::format("in state `{}`", state.name);
						auto const* slot = changeable_slot(name, where);
						if (changes.size() > 1) report(std::format("sets slot `{}` {} twice", name, where));
						if (!slot) continue;
						for (auto const& change : changes) {
							if (!accepts(*slot, change.set)) {
								report(std::format("sets slot `{}` {} to a value, which its type does not accept", name,
								                   where));
							}
						}
					}
				}

				// with the whole order known, every list must follow it
				for (auto const& state : states) {
					for (auto const& [group, changes] : state.groups) {
						auto const& order = orders[group];
						size_t last = 0;
						for (auto const& change : changes) {
							auto const position =
							    static_cast<size_t>(std::ranges::find(order, channel_of(change)) - order.begin());
							if (position < last) {
								report(std::format(
								    "lists the channels of group `{}` in state `{}` in another order than before",
								    group, state.name));
								break;
							}
							last = position;
						}
					}
				}
			}

			// ---- actions -------------------------------------------------------

			void check_actions(std::vector<decl::action> const& actions, decl::states const& states) {
				auto const declared = [&](std::string_view name) {
					return std::ranges::find(states, name, &decl::state::name) != states.end();
				};
				for (auto const& action : actions) {
					auto const what = std::format("action `{}`", action.label);
					if (action.when.empty()) report(std::format("has {} leaving no state", what));
					for (auto const& name : action.when) {
						if (!declared(name))
							report(std::format("has {} leaving state `{}`, which it does not declare", what, name));
					}
					if (!declared(action.then))
						report(
						    std::format("has {} leading to state `{}`, which it does not declare", what, action.then));
				}
			}

			// ---- transitions ---------------------------------------------------

			template <size_t Length>
			void check_rule(std::string const& what,
			                decl::transition const& rule,
			                std::array<std::string_view, Length> const& channels) {
				auto const result = parse_transition(rule);
				if (!result.error.empty()) {
					report(std::format("has {}, which {}", what, result.error));
					return;
				}
				for (auto const& item : result.items) {
					if (std::ranges::find(channels, std::string_view{item.channel}) == channels.end())
						report(std::format("has {}, which names channel `{}` it does not have", what, item.channel));
				}
			}

			void check_transitions(decl::transitions const& rules) {
				// under "*", a channel is skipped for the targets without it
				if (rules.defaults) {
					auto const result = parse_transition(*rules.defaults);
					if (!result.error.empty()) report(std::format("has a default transition, which {}", result.error));
				}
				for (auto const& [group, rule] : rules.groups) {
					check_group(group, "in `transitions`"sv);
					check_rule(std::format("a transition of group `{}`", group), rule, group_channels);
				}
				for (auto const& [name, rule] : rules.slots) {
					changeable_slot(name, "in `transitions`"sv);
					check_rule(std::format("a transition of slot `{}`", name), rule, value_channels);
				}
			}

			model_names const& model_;
			std::vector<problem>& out_;
			size_t index_;
			std::vector<decl::slot> const* slots_{};
			std::vector<std::string> reported_{};
		};
	}  // namespace

	std::vector<problem> check(gfx::interactions::active_objects const& objects, model_names const& model) {
		std::vector<problem> result{};

		for (auto const& [name, count] : model.lights) {
			if (name.find_first_of("./"sv) != std::string::npos)
				result.push_back({.reason = std::format("light `{}` has a `.` or a `/` in its name", name)});
			if (count > 1) result.push_back({.reason = std::format("{} lights share the name `{}`", count, name)});
		}

		std::map<std::string_view, size_t> described{};
		for (auto const& item : objects) {
			if (auto const* light = std::get_if<decl::light_active_object>(&item)) ++described[light->name];
		}

		for (size_t index = 0; index < objects.size(); ++index) {
			object_checker checker{model, result, index};
			std::visit([&](auto const& item) { checker.check(item); }, objects[index]);

			if (auto const* light = std::get_if<decl::light_active_object>(&objects[index])) {
				if (auto const it = model.lights.find(light->name); it != model.lights.end() && it->second > 1)
					result.push_back({.object = index, .reason = "describes a light, whose name is not unique"s});
				if (described[light->name] > 1)
					result.push_back({.object = index, .reason = "describes a light, which another object describes"s});
			}
		}

		// the problems of the model first, then the ones of the objects
		std::ranges::stable_partition(result, [](problem const& item) { return !item.object; });
		return result;
	}

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
}  // namespace pyxel::io::interactions
