// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <array>
#include <map>
#include <pyxel/gfx/loaded_object.hpp>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace std::literals;

namespace pyxel::gfx::interactions {
	namespace {
		constexpr std::array channel_names{
		    "translate.x"sv, "translate.y"sv, "translate.z"sv, "translate"sv, "rotate.x"sv, "rotate.y"sv,
		    "rotate.z"sv,    "rotate"sv,      "scale.x"sv,     "scale.y"sv,   "scale.z"sv,  "scale"sv,
		};

		constexpr auto all_channels = "all"sv;
		constexpr auto set_channel = "set"sv;

		group_channel on_axis(group_channel x_channel, axis on) noexcept {
			return static_cast<group_channel>(std::to_underlying(x_channel) + std::to_underlying(on));
		}

		// The items of the value, or none, if it does not parse.
		std::vector<single_transition> items_of(transition const& value) {
			auto result = parse_transition(value);
			if (!result.error.empty()) return {};
			return std::move(result.items);
		}

		// The last of the items applying to the channel wins; the ones for
		// another channel are skipped, as under "*".
		void apply(timing& result, std::vector<single_transition> const& items, std::string_view channel) {
			for (auto const& item : items) {
				if (item.channel == all_channels || item.channel == channel) result = item.timing();
			}
		}

		template <typename Map>
		std::vector<single_transition> items_for(Map const& rules, typename Map::key_type const& target) {
			auto const it = rules.find(target);
			return it == rules.end() ? std::vector<single_transition>{} : items_of(it->second);
		}

		class geo_loader {
		public:
			explicit geo_loader(geo_active_object const& object) : object_{object} {
				result_.name = object.name;
				result_.kind = object_kind::geometry;
			}

			loaded_object load() && {
				load_slots();
				load_states();
				load_channels();
				load_values();
				load_transitions();
				load_machine();
				load_highlights();
				load_storage();
				return std::move(result_);
			}

		private:
			void load_slots() {
				result_.slots = object_.slots;
				for (auto& slot : result_.slots)
					slot.default_value = default_of(slot);
			}

			// a state declared twice keeps the first declaration
			void load_states() {
				for (auto const& declared : object_.states) {
					if (result_.find_state(declared.name)) continue;
					result_.states.push_back({.name = declared.name});
					declarations_.push_back(&declared);
				}
				if (!result_.states.empty()) result_.start = 0;
			}

			void load_channels() {
				std::map<std::string_view, size_t> groups{};
				for (auto const* declared : declarations_) {
					// the mapping of a state is unordered: the groups new to
					// this state come in the order of their names
					std::vector<std::string_view> names{};
					for (auto const& [group, changes] : declared->groups)
						names.push_back(group);
					std::ranges::sort(names);

					for (auto const name : names) {
						auto [it, added] = groups.try_emplace(name, result_.groups.size());
						if (added) result_.groups.push_back({.group = std::string{name}});
						auto& channels = result_.groups[it->second].channels;
						for (auto const& change : declared->groups.at(std::string{name})) {
							auto const channel = channel_of(change);
							if (std::ranges::find(channels, channel) == channels.end()) channels.push_back(channel);
						}
					}
				}

				// the declared order of the slots; a state may only set an
				// output slot or a variable
				for (size_t index = 0; index < result_.slots.size(); ++index) {
					auto const& slot = result_.slots[index];
					if (slot.direction == direction::input) continue;
					auto const set = std::ranges::any_of(
					    declarations_, [&](state const* declared) { return declared->slots.contains(slot.name); });
					if (set) result_.slot_channels.push_back({.slot = index});
				}
			}

			void load_values() {
				for (size_t index = 0; index < result_.states.size(); ++index) {
					auto const& declared = *declarations_[index];
					auto& values = result_.states[index].values;

					values.groups.reserve(result_.groups.size());
					for (auto const& group : result_.groups) {
						auto& channels = values.groups.emplace_back();
						channels.reserve(group.channels.size());
						for (auto const channel : group.channels)
							channels.push_back(default_of(channel));

						auto const it = declared.groups.find(group.group);
						if (it == declared.groups.end()) continue;
						for (auto const& change : it->second) {
							auto const pos =
							    std::ranges::find(group.channels, channel_of(change)) - group.channels.begin();
							channels[static_cast<size_t>(pos)] = change;
						}
					}

					values.slots.reserve(result_.slot_channels.size());
					for (auto const& channel : result_.slot_channels) {
						auto const& slot = result_.slots[channel.slot];
						auto& value = values.slots.emplace_back(*slot.default_value);
						auto const it = declared.slots.find(slot.name);
						if (it != declared.slots.end() && !it->second.empty()) value = it->second.back().set;
					}
				}
			}

			// "*" first, then the target, a later item winning
			void load_transitions() {
				auto const& rules = object_.transitions;
				auto const defaults = rules.defaults ? items_of(*rules.defaults) : std::vector<single_transition>{};

				for (auto& group : result_.groups) {
					auto const own = items_for(rules.groups, group.group);
					group.transitions.reserve(group.channels.size());
					for (auto const channel : group.channels) {
						auto& result = group.transitions.emplace_back();
						apply(result, defaults, name_of(channel));
						apply(result, own, name_of(channel));
					}
				}

				for (auto& channel : result_.slot_channels) {
					apply(channel.transition, defaults, set_channel);
					apply(channel.transition, items_for(rules.slots, result_.slots[channel.slot].name), set_channel);
				}
			}

			// the first action available in a state is its default
			// interaction
			void load_machine() {
				result_.actions.reserve(object_.actions.size());
				for (size_t index = 0; index < object_.actions.size(); ++index) {
					auto const& action = object_.actions[index];
					result_.actions.push_back(action.label);

					auto const then = result_.find_state(action.then);
					if (!then) continue;
					for (auto const& when : action.when) {
						auto const from = result_.find_state(when);
						if (!from) continue;
						auto& machine = result_.states[*from].machine;
						machine.try_emplace(static_cast<std::ptrdiff_t>(index), *then);
						machine.try_emplace(default_interaction, *then);
					}
				}
			}

			void load_highlights() {
				for (auto& state : result_.states) {
					std::set<size_t> moving{};
					for (auto const& [action, target] : state.machine) {
						auto const& there = result_.states[target].values.groups;
						for (size_t group = 0; group < there.size(); ++group) {
							if (state.values.groups[group] != there[group]) moving.insert(group);
						}
					}
					state.highlights.assign(moving.begin(), moving.end());
				}
			}

			void load_storage() {
				auto& storage = result_.storage;
				storage.state = result_.start;
				storage.progress = 1;

				if (result_.start) {
					storage.channels.groups = result_.states[*result_.start].values.groups;
				} else {
					// with no states, there are no channels either
					storage.channels.groups.resize(result_.groups.size());
				}

				storage.slots.reserve(result_.slots.size());
				for (auto const& slot : result_.slots)
					storage.slots.push_back(*slot.default_value);
				if (result_.start) {
					auto const& values = result_.states[*result_.start].values.slots;
					for (size_t index = 0; index < result_.slot_channels.size(); ++index)
						storage.slots[result_.slot_channels[index].slot] = values[index];
				}
			}

			geo_active_object const& object_;
			loaded_object result_{};
			// the declaration of each loaded state
			std::vector<state const*> declarations_{};
		};
	}  // namespace

	group_channel channel_of(group_transformation const& change) noexcept {
		struct visitor {
			group_channel operator()(translate_axis const& item) const noexcept {
				return on_axis(group_channel::translate_x, item.on);
			}
			group_channel operator()(translate_all const&) const noexcept { return group_channel::translate; }
			group_channel operator()(rotate_axis const& item) const noexcept {
				return on_axis(group_channel::rotate_x, item.around);
			}
			group_channel operator()(rotate_all const&) const noexcept { return group_channel::rotate; }
			group_channel operator()(scale_axis const& item) const noexcept {
				return on_axis(group_channel::scale_x, item.along);
			}
			group_channel operator()(scale_all const&) const noexcept { return group_channel::scale; }
		};
		return std::visit(visitor{}, change);
	}

	std::string_view name_of(group_channel channel) noexcept { return channel_names[std::to_underlying(channel)]; }

	group_transformation default_of(group_channel channel) noexcept {
		switch (channel) {
			case group_channel::translate_x:
				return translate_axis{.on = axis::x};
			case group_channel::translate_y:
				return translate_axis{.on = axis::y};
			case group_channel::translate_z:
				return translate_axis{.on = axis::z};
			case group_channel::translate:
				return translate_all{};
			case group_channel::rotate_x:
				return rotate_axis{.around = axis::x};
			case group_channel::rotate_y:
				return rotate_axis{.around = axis::y};
			case group_channel::rotate_z:
				return rotate_axis{.around = axis::z};
			case group_channel::rotate:
				return rotate_all{};
			case group_channel::scale_x:
				return scale_axis{.along = axis::x};
			case group_channel::scale_y:
				return scale_axis{.along = axis::y};
			case group_channel::scale_z:
				return scale_axis{.along = axis::z};
			case group_channel::scale:
				break;
		}
		return scale_all{};
	}

	slot_value default_of(slot const& declared) noexcept {
		if (declared.default_value) return *declared.default_value;
		switch (declared.type) {
			case slot_type::boolean:
				return false;
			case slot_type::floating:
			case slot_type::factor:
				return 0.0;
			case slot_type::angle:
				return angle{};
			case slot_type::length:
				return length{};
			case slot_type::offset:
				break;
		}
		return offset{};
	}

	std::optional<size_t> loaded_object::find_state(std::string_view state) const noexcept {
		auto const it = std::ranges::find(states, state, &loaded_state::name);
		if (it == states.end()) return std::nullopt;
		return static_cast<size_t>(it - states.begin());
	}

	loaded_object load(geo_active_object const& object) { return geo_loader{object}.load(); }

	// The two input slots and the update expression, `enabled ? intensity :
	// 0`, are synthesized; the object only brings the transition of the
	// intensity factor, its only channel.
	loaded_object load(light_active_object const& object) {
		loaded_object result{.name = object.name, .kind = object_kind::light};
		result.slots = {
		    slot{"enabled", slot_type::boolean, direction::input, true},
		    slot{"intensity", slot_type::factor, direction::input, 1.0},
		};
		apply(result.light, items_of(object.light), set_channel);

		result.storage.light = 1.0;
		result.storage.slots = {true, 1.0};
		return result;
	}

	loaded_objects load(active_objects const& objects, std::span<std::string const> light_names) {
		loaded_objects result{};
		std::set<std::string_view> described{};
		for (auto const& object : objects) {
			if (auto const* light = std::get_if<light_active_object>(&object)) described.insert(light->name);
			result.push_back(std::visit([](auto const& item) { return load(item); }, object));
		}

		// lights sharing a name cannot be told apart, so none of them is an
		// active object
		std::map<std::string_view, size_t> counts{};
		for (auto const& name : light_names)
			++counts[name];
		for (auto const& name : light_names) {
			if (name.empty() || counts[name] > 1 || described.contains(name)) continue;
			result.push_back(load(light_active_object{.name = name}));
		}
		return result;
	}
}  // namespace pyxel::gfx::interactions
