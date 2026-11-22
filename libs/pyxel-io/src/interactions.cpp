// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <array>
#include <pyxel/io/interactions.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using namespace std::literals;

namespace pyxel::io::interactions {
	namespace {
		namespace decl = gfx::interactions;
		using writer_t = rapidjson::Writer<rapidjson::StringBuffer>;

		static constexpr auto version = "1.0"sv;
		static constexpr auto slot_prefix = "$"sv;

		template <typename... Ts>
		struct overloaded : Ts... {
			using Ts::operator()...;
		};

		struct json_array {
			writer_t& out_;

			explicit json_array(writer_t& out) : out_{out} { out_.StartArray(); }
			~json_array() { out_.EndArray(); }
		};

		struct json_object {
			writer_t& out_;

			explicit json_object(writer_t& out) : out_{out} { out_.StartObject(); }
			~json_object() { out_.EndObject(); }
		};

		void key(writer_t& out, std::string_view name) {
			out.Key(name.data(), static_cast<rapidjson::SizeType>(name.size()));
		}

		void string(writer_t& out, std::string_view value) {
			out.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
		}

		std::string concat(std::string_view a, std::string_view b) {
			std::string result{};
			result.reserve(a.length() + b.length());
			result.append(a);
			result.append(b);
			return result;
		}

		// "$" and the declared name; a name starting with "$" gets one more
		std::string slot_ref(std::string_view const& name) { return concat(slot_prefix, name); }

		// the entries of an unordered map, by their keys
		template <typename Value>
		std::vector<std::pair<std::string_view, Value const*>> sorted(
		    std::unordered_map<std::string, Value> const& map) {
			std::vector<std::pair<std::string_view, Value const*>> result{};
			result.reserve(map.size());
			for (auto const& [name, value] : map)
				result.emplace_back(name, &value);
			std::ranges::sort(result, [](auto const& lhs, auto const& rhs) { return lhs.first < rhs.first; });
			return result;
		}

		// the vector channel of a kind, and its single-axis channels, by axis
		struct channel_names {
			std::string_view all;
			std::array<std::string_view, 3> axes;

			constexpr std::string_view on(gfx::axis axis) const noexcept { return axes[std::to_underlying(axis)]; }
		};

		constexpr channel_names translate_names{"translate"sv, {"translate.x"sv, "translate.y"sv, "translate.z"sv}};
		constexpr channel_names rotate_names{"rotate"sv, {"rotate.x"sv, "rotate.y"sv, "rotate.z"sv}};
		constexpr channel_names scale_names{"scale"sv, {"scale.x"sv, "scale.y"sv, "scale.z"sv}};

		void vector(writer_t& out, double x, double y, double z) {
			json_array in{out};
			out.Double(x);
			out.Double(y);
			out.Double(z);
		}

		void offset(writer_t& out, gfx::offset const& value) {
			vector(out, value.x.count(), value.y.count(), value.z.count());
		}

		void channel(writer_t& out, channel_names const& names, gfx::axis on, double value) {
			json_object transformation{out};
			key(out, names.on(on));
			out.Double(value);
		}

		void channel(writer_t& out, channel_names const& names, double x, double y, double z) {
			json_object transformation{out};
			key(out, names.all);
			vector(out, x, y, z);
		}

		void transformation(writer_t& out, decl::group_transformation const& change) {
			std::visit(
			    overloaded{
			        [&](decl::translate_axis const& item) { channel(out, translate_names, item.on, item.by.count()); },
			        [&](decl::translate_all const& item) {
				        channel(out, translate_names, item.by.x.count(), item.by.y.count(), item.by.z.count());
			        },
			        [&](decl::rotate_axis const& item) { channel(out, rotate_names, item.around, item.by.count()); },
			        [&](decl::rotate_all const& item) {
				        channel(out, rotate_names, item.x.count(), item.y.count(), item.z.count());
			        },
			        [&](decl::scale_axis const& item) { channel(out, scale_names, item.along, item.by); },
			        [&](decl::scale_all const& item) { channel(out, scale_names, item.x, item.y, item.z); },
			    },
			    change);
		}

		void value(writer_t& out, decl::slot_value const& item) {
			std::visit(overloaded{
			               [&](bool flag) { out.Bool(flag); },
			               [&](double number) { out.Double(number); },
			               [&](gfx::angle const& angle) { out.Double(angle.count()); },
			               [&](gfx::length const& length) { out.Double(length.count()); },
			               [&](gfx::offset const& off) { offset(out, off); },
			           },
			           item);
		}

		std::string_view name_of(decl::slot_type type) noexcept {
			switch (type) {
				case decl::slot_type::factor:
					return "factor"sv;
				case decl::slot_type::boolean:
					return "boolean"sv;
				case decl::slot_type::angle:
					return "angle"sv;
				case decl::slot_type::length:
					return "length"sv;
				case decl::slot_type::offset:
					return "offset"sv;
				case decl::slot_type::floating:
					break;
			}
			return "float"sv;
		}

		std::string_view name_of(decl::direction direction) noexcept {
			switch (direction) {
				case decl::direction::output:
					return "output"sv;
				case decl::direction::variable:
					return "variable"sv;
				case decl::direction::input:
					break;
			}
			return "input"sv;
		}

		void slots(writer_t& out, std::vector<decl::slot> const& list) {
			if (list.empty()) return;
			key(out, "slots"sv);
			json_array in{out};

			for (auto const& item : list) {
				json_object slot{out};
				key(out, "name"sv);
				string(out, item.name);
				key(out, "type"sv);
				string(out, name_of(item.type));
				key(out, "direction"sv);
				string(out, name_of(item.direction));
				if (item.default_value) {
					key(out, "default"sv);
					value(out, *item.default_value);
				}
			}
		}

		void states(writer_t& out, decl::states const& list) {
			if (list.empty()) return;
			key(out, "states"sv);
			json_object in{out};

			for (auto const& item : list) {
				key(out, item.name);
				json_object state{out};

				for (auto const& [group, changes] : sorted(item.groups)) {
					key(out, group);
					json_array transformations{out};
					for (auto const& change : *changes)
						transformation(out, change);
				}

				for (auto const& [slot, changes] : sorted(item.slots)) {
					key(out, slot_ref(slot));
					json_array transformations{out};
					for (auto const& change : *changes) {
						json_object transformation{out};
						key(out, "set"sv);
						value(out, change.set);
					}
				}
			}
		}

		void actions(writer_t& out, std::vector<decl::action> const& list) {
			if (list.empty()) return;
			key(out, "actions"sv);
			json_array in{out};

			for (auto const& item : list) {
				json_object action{out};
				key(out, "label"sv);
				string(out, item.label);
				key(out, "when"sv);
				// a single name is written as a string, as it is read
				if (item.when.size() == 1) {
					string(out, item.when.front());
				} else {
					json_array when{out};
					for (auto const& name : item.when)
						string(out, name);
				}
				key(out, "then"sv);
				string(out, item.then);
			}
		}

		void transitions(writer_t& out, decl::transitions const& rules) {
			if (!rules.defaults && rules.groups.empty() && rules.slots.empty()) return;
			key(out, "transitions"sv);
			json_object in{out};

			if (rules.defaults) {
				key(out, "*"sv);
				string(out, *rules.defaults);
			}
			for (auto const& [group, rule] : sorted(rules.groups)) {
				key(out, group);
				string(out, *rule);
			}
			for (auto const& [slot, rule] : sorted(rules.slots)) {
				key(out, slot_ref(slot));
				string(out, *rule);
			}
		}

		void header(writer_t& out, std::string const& name) {
			key(out, "name"sv);
			string(out, name);
			key(out, "version"sv);
			string(out, version);
		}

		void object(writer_t& out, decl::geo_active_object const& item) {
			json_object in{out};
			header(out, item.name);
			slots(out, item.slots);
			states(out, item.states);
			actions(out, item.actions);
			transitions(out, item.transitions);
		}

		void object(writer_t& out, decl::light_active_object const& item) {
			json_object in{out};
			header(out, item.name);
			if (!item.light.empty()) {
				key(out, "transitions"sv);
				json_object transitions{out};
				key(out, "light"sv);
				string(out, item.light);
			}
		}
	}  // namespace

	std::string serialize(gfx::interactions::active_objects const& objects) {
		rapidjson::StringBuffer text{};
		{
			// the writer and the array are closed before the text is read
			writer_t out{text};
			json_array in{out};
			for (auto const& item : objects)
				std::visit([&](auto const& alternative) { object(out, alternative); }, item);
		}
		return {text.GetString(), text.GetSize()};
	}
}  // namespace pyxel::io::interactions
