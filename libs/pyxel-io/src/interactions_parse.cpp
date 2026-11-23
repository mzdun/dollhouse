// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <rapidjson/error/en.h>
#include <algorithm>
#include <charconv>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "interactions_json.hpp"

using namespace std::literals;

namespace pyxel::io::interactions {
	namespace {
		namespace decl = gfx::interactions;
		using json = rapidjson::Value;

		constexpr char slot_prefix = '$';
		constexpr auto defaults_target = "*"sv;
		constexpr auto light_target = "light"sv;

		// thrown while reading an object, which is then dropped
		struct drop {
			std::string reason;
		};

		[[noreturn]] void fail(std::string reason) { throw drop{std::move(reason)}; }

		std::string_view view(json const& value) { return {value.GetString(), value.GetStringLength()}; }

		json const* member(json const& object, std::string_view name) {
			auto const it = object.FindMember(json{rapidjson::StringRef(name.data(), name.size())});
			return it == object.MemberEnd() ? nullptr : &it->value;
		}

		// ---- versions ------------------------------------------------------

		struct version_t {
			unsigned major;
			unsigned minor;
		};

		constexpr version_t own_version{1, 0};

		// a number as in SemVer: digits, with no leading zero
		std::optional<unsigned> semver_number(std::string_view text) {
			if (text.empty() || (text.size() > 1 && text.front() == '0')) return std::nullopt;
			unsigned result{};
			auto const* const end = text.data() + text.size();
			auto const [ptr, ec] = std::from_chars(text.data(), end, result);
			if (ec != std::errc{} || ptr != end) return std::nullopt;
			return result;
		}

		// (0|[1-9][0-9]*)\.(0|[1-9][0-9]*)
		std::optional<version_t> parse_version(std::string_view text) {
			auto const dot = text.find('.');
			if (dot == std::string_view::npos) return std::nullopt;
			auto const major = semver_number(text.substr(0, dot));
			auto const minor = semver_number(text.substr(dot + 1));
			if (!major || !minor) return std::nullopt;
			return version_t{*major, *minor};
		}

		// ---- one object ----------------------------------------------------

		struct channel_kind {
			std::string_view name;
			enum { translate, rotate, scale } kind;
			std::optional<gfx::axis> axis;
		};

		constexpr channel_kind channels[] = {
		    {"translate.x"sv, channel_kind::translate, gfx::axis::x},
		    {"translate.y"sv, channel_kind::translate, gfx::axis::y},
		    {"translate.z"sv, channel_kind::translate, gfx::axis::z},
		    {"translate"sv, channel_kind::translate, std::nullopt},
		    {"rotate.x"sv, channel_kind::rotate, gfx::axis::x},
		    {"rotate.y"sv, channel_kind::rotate, gfx::axis::y},
		    {"rotate.z"sv, channel_kind::rotate, gfx::axis::z},
		    {"rotate"sv, channel_kind::rotate, std::nullopt},
		    {"scale.x"sv, channel_kind::scale, gfx::axis::x},
		    {"scale.y"sv, channel_kind::scale, gfx::axis::y},
		    {"scale.z"sv, channel_kind::scale, gfx::axis::z},
		    {"scale"sv, channel_kind::scale, std::nullopt},
		};

		std::optional<decl::slot_type> slot_type_of(std::string_view name) {
			if (name == "float"sv) return decl::slot_type::floating;
			if (name == "factor"sv) return decl::slot_type::factor;
			if (name == "boolean"sv) return decl::slot_type::boolean;
			if (name == "angle"sv) return decl::slot_type::angle;
			if (name == "length"sv) return decl::slot_type::length;
			if (name == "offset"sv) return decl::slot_type::offset;
			return std::nullopt;
		}

		std::optional<decl::direction> direction_of(std::string_view name) {
			if (name == "input"sv) return decl::direction::input;
			if (name == "output"sv) return decl::direction::output;
			if (name == "variable"sv) return decl::direction::variable;
			return std::nullopt;
		}

		std::optional<gfx::offset> offset_of(json const& value) {
			if (!value.IsArray() || value.Size() != 3) return std::nullopt;
			for (auto const& item : value.GetArray()) {
				if (!item.IsNumber()) return std::nullopt;
			}
			return gfx::offset{gfx::length{value[0].GetDouble()}, gfx::length{value[1].GetDouble()},
			                   gfx::length{value[2].GetDouble()}};
		}

		// a value of the type, or nothing, if the JSON does not hold one
		std::optional<decl::slot_value> value_of(json const& value, decl::slot_type type) {
			switch (type) {
				case decl::slot_type::boolean:
					if (value.IsBool()) return decl::slot_value{value.GetBool()};
					break;
				case decl::slot_type::floating:
				case decl::slot_type::factor:
					if (value.IsNumber()) return decl::slot_value{value.GetDouble()};
					break;
				case decl::slot_type::angle:
					if (value.IsNumber()) return decl::slot_value{gfx::rad(value.GetDouble())};
					break;
				case decl::slot_type::length:
					if (value.IsNumber()) return decl::slot_value{gfx::length{value.GetDouble()}};
					break;
				case decl::slot_type::offset:
					if (auto const off = offset_of(value)) return decl::slot_value{*off};
					break;
			}
			return std::nullopt;
		}

		// a value of an undeclared slot: its type is guessed from the JSON,
		// leaving the slot to the rules
		std::optional<decl::slot_value> guessed_value_of(json const& value) {
			if (value.IsBool()) return decl::slot_value{value.GetBool()};
			if (value.IsNumber()) return decl::slot_value{value.GetDouble()};
			if (auto const off = offset_of(value)) return decl::slot_value{*off};
			return std::nullopt;
		}

		class object_reader {
		public:
			// `lenient` for a newer minor version, which drops what it does
			// not know, instead of failing on it
			object_reader(bool lenient) : lenient_{lenient} {}

			std::vector<std::string> const& ignored() const noexcept { return ignored_; }

			decl::light_active_object light(json const& object, std::string name) {
				decl::light_active_object result{.name = std::move(name)};
				for (auto const& [key_value, value] : object.GetObject()) {
					auto const key = view(key_value);
					if (key == "name"sv || key == "version"sv) continue;
					if (key == "slots"sv || key == "states"sv || key == "actions"sv)
						fail(std::format("describes a light, but has `{}`", key));
					if (key != "transitions"sv) {
						unknown(std::format("key `{}`", key));
						continue;
					}

					if (!value.IsObject()) fail("has `transitions`, which are not a mapping"s);
					for (auto const& [target_value, rule] : value.GetObject()) {
						auto const target = view(target_value);
						if (target != light_target)
							fail(std::format("describes a light, but has a transition of `{}`", target));
						if (!rule.IsString()) fail("has a transition of `light`, which is not a string"s);
						result.light = view(rule);
					}
				}
				return result;
			}

			decl::geo_active_object geo(json const& object, std::string name) {
				decl::geo_active_object result{.name = std::move(name)};

				// the slots first: the states need their types
				if (auto const* value = member(object, "slots"sv)) result.slots = slots(*value);

				for (auto const& [key_value, value] : object.GetObject()) {
					auto const key = view(key_value);
					if (key == "name"sv || key == "version"sv || key == "slots"sv) continue;
					if (key == "states"sv) {
						result.states = states(value, result.slots);
					} else if (key == "actions"sv) {
						result.actions = actions(value);
					} else if (key == "transitions"sv) {
						result.transitions = transitions(value);
					} else {
						unknown(std::format("key `{}`", key));
					}
				}
				return result;
			}

		private:
			void unknown(std::string what) {
				if (!lenient_) fail(std::format("has an unknown {}", what));
				ignored_.push_back(std::move(what));
			}

			// ---- slots ---------------------------------------------------------

			std::vector<decl::slot> slots(json const& value) {
				if (!value.IsArray()) fail("has `slots`, which are not a list"s);
				std::vector<decl::slot> result{};
				for (auto const& item : value.GetArray()) {
					if (auto read = slot(item)) result.push_back(std::move(*read));
				}
				return result;
			}

			std::string required_string(json const& object, std::string_view key, std::string_view what) {
				auto const* value = member(object, key);
				if (!value || !value->IsString()) fail(std::format("has {} with no `{}`", what, key));
				return std::string{view(*value)};
			}

			std::optional<decl::slot> slot(json const& item) {
				if (!item.IsObject()) fail("has a slot, which is not a mapping"s);
				auto name = required_string(item, "name"sv, "a slot"sv);
				auto const what = std::format("slot `{}`", name);
				auto const type_name = required_string(item, "type"sv, what);
				auto const direction_name = required_string(item, "direction"sv, what);

				for (auto const& [key_value, value] : item.GetObject()) {
					auto const key = view(key_value);
					if (key != "name"sv && key != "type"sv && key != "direction"sv && key != "default"sv)
						unknown(std::format("key `{}` of {}", key, what));
				}

				auto const type = slot_type_of(type_name);
				auto const direction = direction_of(direction_name);
				if (!type || !direction) {
					// a newer minor version may know them; the slot goes,
					// together with every reference to it
					unknown(!type ? std::format("type `{}` of {}", type_name, what)
					              : std::format("direction `{}` of {}", direction_name, what));
					removed_.insert(name);
					return std::nullopt;
				}

				std::optional<decl::slot_value> default_value{};
				if (auto const* value = member(item, "default"sv)) {
					default_value = value_of(*value, *type);
					if (!default_value) fail(std::format("has {} with a default, which is not a {}", what, type_name));
				}
				return decl::slot{std::move(name), *type, *direction, std::move(default_value)};
			}

			bool removed(std::string_view slot) const { return removed_.contains(slot); }

			// ---- states --------------------------------------------------------

			decl::states states(json const& value, std::vector<decl::slot> const& declared) {
				if (!value.IsObject()) fail("has `states`, which are not a mapping"s);
				decl::states result{};
				for (auto const& [name_value, body] : value.GetObject()) {
					auto const name = view(name_value);
					if (!body.IsObject()) fail(std::format("has state `{}`, which is not a mapping", name));

					decl::state item{std::string{name}};
					for (auto const& [target_value, changes] : body.GetObject()) {
						auto const target = view(target_value);
						auto const where = std::format("`{}` in state `{}`", target, name);
						if (!changes.IsArray()) fail(std::format("has {}, which is not a list", where));

						if (target.starts_with(slot_prefix)) {
							auto const slot = target.substr(1);
							if (removed(slot)) continue;
							slot_changes(item, slot, changes, declared, where);
						} else {
							group_changes(item, target, changes, where);
						}
					}
					result.push_back(std::move(item));
				}
				return result;
			}

			// the only member of a single-key mapping
			json::ConstMemberIterator single(json const& change, std::string_view where) {
				if (!change.IsObject() || change.MemberCount() != 1)
					fail(std::format("has a transformation of {}, which does not set a single channel", where));
				return change.MemberBegin();
			}

			void slot_changes(decl::state& item,
			                  std::string_view slot,
			                  json const& changes,
			                  std::vector<decl::slot> const& declared,
			                  std::string_view where) {
				auto const it = std::ranges::find(declared, slot, &decl::slot::name);
				std::string const name{slot};
				auto& list = item.slots[name];
				for (auto const& change : changes.GetArray()) {
					auto const channel = single(change, where);
					auto const channel_name = view(channel->name);
					if (channel_name != "set"sv) {
						unknown(std::format("channel `{}` of {}", channel_name, where));
						continue;
					}
					auto value =
					    it != declared.end() ? value_of(channel->value, it->type) : guessed_value_of(channel->value);
					if (!value) fail(std::format("sets {} to a value of a wrong type", where));
					list.push_back({.set = std::move(*value)});
				}
				// every change was left out, or there were none: the state does
				// not set the slot
				if (list.empty()) item.slots.erase(name);
			}

			void group_changes(decl::state& item, std::string_view group, json const& changes, std::string_view where) {
				std::string const key{group};
				auto& list = item.groups[key];
				for (auto const& change : changes.GetArray()) {
					auto const member_it = single(change, where);
					auto const name = view(member_it->name);
					auto const known = std::ranges::find(channels, name, &channel_kind::name);
					if (known == std::end(channels)) {
						unknown(std::format("channel `{}` of {}", name, where));
						continue;
					}
					list.push_back(transformation(*known, member_it->value, where));
				}
				// as for the slots: no transformation left, no group
				if (list.empty()) item.groups.erase(key);
			}

			decl::group_transformation transformation(channel_kind const& channel,
			                                          json const& value,
			                                          std::string_view where) {
				auto const wrong = [&]() -> decl::group_transformation {
					fail(std::format("sets `{}` of {} to a value of a wrong type", channel.name, where));
				};

				if (channel.axis) {
					if (!value.IsNumber()) return wrong();
					auto const number = value.GetDouble();
					switch (channel.kind) {
						case channel_kind::translate:
							return decl::translate_axis{.on = *channel.axis, .by = gfx::length{number}};
						case channel_kind::rotate:
							return decl::rotate_axis{.around = *channel.axis, .by = gfx::rad(number)};
						case channel_kind::scale:
							return decl::scale_axis{.along = *channel.axis, .by = number};
					}
				}

				// one to three numbers; the missing ones take the default value
				if (!value.IsArray() || value.Empty() || value.Size() > 3) return wrong();
				auto const fallback = channel.kind == channel_kind::scale ? 1.0 : 0.0;
				double xyz[3]{fallback, fallback, fallback};
				for (rapidjson::SizeType index = 0; index < value.Size(); ++index) {
					if (!value[index].IsNumber()) return wrong();
					xyz[index] = value[index].GetDouble();
				}
				switch (channel.kind) {
					case channel_kind::translate:
						return decl::translate_all{
						    .by = {gfx::length{xyz[0]}, gfx::length{xyz[1]}, gfx::length{xyz[2]}}};
					case channel_kind::rotate:
						return decl::rotate_all{.x = gfx::rad(xyz[0]), .y = gfx::rad(xyz[1]), .z = gfx::rad(xyz[2])};
					case channel_kind::scale:
						break;
				}
				return decl::scale_all{.x = xyz[0], .y = xyz[1], .z = xyz[2]};
			}

			// ---- actions -------------------------------------------------------

			std::vector<decl::action> actions(json const& value) {
				if (!value.IsArray()) fail("has `actions`, which are not a list"s);
				std::vector<decl::action> result{};
				for (auto const& item : value.GetArray()) {
					if (!item.IsObject()) fail("has an action, which is not a mapping"s);
					auto label = required_string(item, "label"sv, "an action"sv);
					auto const what = std::format("action `{}`", label);
					auto then = required_string(item, "then"sv, what);

					std::vector<decl::state_name> when{};
					auto const* when_value = member(item, "when"sv);
					if (when_value && when_value->IsString()) {
						when.emplace_back(view(*when_value));
					} else if (when_value && when_value->IsArray() && !when_value->Empty()) {
						for (auto const& name : when_value->GetArray()) {
							if (!name.IsString()) fail(std::format("has {} with a `when`, which is not a name", what));
							when.emplace_back(view(name));
						}
					} else {
						fail(std::format("has {} with no `when`", what));
					}

					for (auto const& [key_value, unused] : item.GetObject()) {
						auto const key = view(key_value);
						if (key != "label"sv && key != "when"sv && key != "then"sv)
							unknown(std::format("key `{}` of {}", key, what));
					}
					result.emplace_back(std::move(label), std::move(when), std::move(then));
				}
				return result;
			}

			// ---- transitions ---------------------------------------------------

			decl::transitions transitions(json const& value) {
				if (!value.IsObject()) fail("has `transitions`, which are not a mapping"s);
				decl::transitions result{};
				for (auto const& [target_value, rule_value] : value.GetObject()) {
					auto const target = view(target_value);
					if (!rule_value.IsString())
						fail(std::format("has a transition of `{}`, which is not a string", target));
					std::string rule{view(rule_value)};

					if (target == defaults_target) {
						result.for_all(std::move(rule));
					} else if (target.starts_with(slot_prefix)) {
						auto const slot = target.substr(1);
						if (!removed(slot)) result.for_slot(std::string{slot}, std::move(rule));
					} else {
						result.for_group(std::string{target}, std::move(rule));
					}
				}
				return result;
			}

			bool lenient_;
			std::vector<std::string> ignored_{};
			std::set<std::string, std::less<>> removed_{};
		};

		std::string join(std::vector<std::string> const& items) {
			std::string result{};
			for (auto const& item : items) {
				if (!result.empty()) result.append(", "sv);
				result.append(item);
			}
			return result;
		}

		void read_object(json const& item, std::span<std::string const> light_names, parsed& out) {
			if (!item.IsObject()) {
				out.notes.push_back({.what = note::dropped, .reason = "the entry is not a mapping"s});
				return;
			}

			std::string name{};
			if (auto const* value = member(item, "name"sv); value && value->IsString()) name = view(*value);
			auto const dropped = [&](std::string reason) {
				out.notes.push_back({.object = name, .what = note::dropped, .reason = std::move(reason)});
			};

			if (!member(item, "name"sv) || !member(item, "name"sv)->IsString()) return dropped("it has no name"s);

			auto const* version_value = member(item, "version"sv);
			if (!version_value || !version_value->IsString()) return dropped("it has no version"s);
			auto const version_text = view(*version_value);
			auto const version = parse_version(version_text);
			if (!version) return dropped(std::format("\"{}\" is not a version", version_text));
			if (version->major > own_version.major)
				return dropped(std::format("version {} is not supported", version_text));
			if (version->major < own_version.major)
				return dropped(std::format("version {} is too old, and cannot be upgraded", version_text));

			object_reader reader{version->minor > own_version.minor};
			try {
				// a light with an empty name is not an active object, so the
				// empty name describes none
				auto const is_light = !name.empty() && std::ranges::find(light_names, name) != light_names.end();
				if (is_light) {
					out.objects.push_back(reader.light(item, name));
				} else {
					out.objects.push_back(reader.geo(item, name));
				}
			} catch (drop const& reason) {
				return dropped(reason.reason);
			}

			if (!reader.ignored().empty()) {
				out.notes.push_back(
				    {.object = name,
				     .what = note::ignored,
				     .reason = std::format("version {} is newer; left out: {}", version_text, join(reader.ignored()))});
			}
		}
	}  // namespace

	void parse_into(rapidjson::Value const& list, std::span<std::string const> light_names, parsed& out) {
		for (auto const& item : list.GetArray())
			read_object(item, light_names, out);
	}

	parsed parse(std::string_view json, std::span<std::string const> light_names) {
		rapidjson::Document doc{};
		doc.Parse(json.data(), json.size());
		if (doc.HasParseError()) {
			throw std::runtime_error(std::format("the {} extra cannot be parsed: {}", "PYXEL_interactions",
			                                     rapidjson::GetParseError_En(doc.GetParseError())));
		}
		if (!doc.IsArray()) throw std::runtime_error("the PYXEL_interactions extra is not a list");

		parsed result{};
		parse_into(doc, light_names, result);
		return result;
	}
}  // namespace pyxel::io::interactions
