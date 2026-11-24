// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "print.hpp"
#include <format>
#include <print>
#include <string>
#include <vector>
#include "names.hpp"
#include "outline.hpp"

using namespace std::literals;

namespace verify {
	namespace {
		namespace gfx = pyxel::gfx;
		using loaded_object = gfx_ia::loaded_object;

		void print_hierarchy(outline const& out, gfx::loaded_model const& model, size_t parent) {
			// the nodes are in depth-first order, so the subtree of `parent`
			// follows it and ends at the first node with an earlier parent
			for (auto index = parent + 1; index < model.nodes.size(); ++index) {
				auto const& node = model.nodes[index];
				if (!node.parent || *node.parent < parent) break;
				if (*node.parent != parent) continue;

				out.line("- {}", node.name);
				print_hierarchy(out.nested(), model, index);
			}
		}

		void print_lights(outline const& out, gfx::loaded_model const& model) {
			for (auto const& light : model.lights) {
				out.line("- name: {}", light.name);
				out.nested().line("type: {}", name_of(light.type));
			}
		}

		std::string action_name(loaded_object const& object, std::ptrdiff_t action) {
			if (action == gfx_ia::default_interaction) return "<click>"s;
			return std::format("\"{}\"", object.actions[static_cast<size_t>(action)]);
		}

		void print_slots(outline const& out, loaded_object const& object) {
			if (object.slots.empty()) return;

			out.line("slots:");
			for (auto const& [name, type, direction, default_value] : object.slots) {
				auto const value = default_value ? std::format(" = {}", format(*default_value)) : ""s;
				out.nested().line("- {} {} {}{}", name_of(direction), name_of(type), name, value);
			}
		}

		void print_actions(outline const& out, loaded_object const& object) {
			if (object.actions.empty()) return;

			out.line("actions:");
			for (auto const& label : object.actions)
				out.nested().line("- \"{}\"", label);
		}

		void print_values(outline const& out, loaded_object const& object, gfx_ia::channel_values const& values) {
			if (values.groups.empty() && values.slots.empty()) return;

			out.line("values:");
			auto const items = out.nested();
			for (size_t index = 0; index < values.groups.size() && index < object.groups.size(); ++index) {
				std::string changes{};
				for (auto const& change : values.groups[index]) {
					if (!changes.empty()) changes.append(", "sv);
					changes.append(format(change));
				}
				items.line("{}: {{{}}}", object.groups[index].group, changes);
			}
			for (size_t index = 0; index < values.slots.size() && index < object.slot_channels.size(); ++index) {
				auto const& slot = object.slots[object.slot_channels[index].slot];
				items.line("${}: {}", slot.name, format(values.slots[index]));
			}
		}

		void print_state(outline const& out, loaded_object const& object, gfx_ia::loaded_state const& state) {
			out.line("- name: {}", state.name);
			auto const fields = out.nested();

			if (!state.machine.empty()) {
				fields.line("state machine:");
				for (auto const& [action, end] : state.machine)
					fields.nested().line("on {}: {}", action_name(object, action), object.states[end].name);
			}

			if (!state.highlights.empty()) {
				fields.line("highlights:");
				for (auto const index : state.highlights)
					fields.nested().line("- {}", object.groups[index].group);
			}

			print_values(fields, object, state.values);
		}

		void print_states(outline const& out, loaded_object const& object) {
			if (object.states.empty()) return;

			out.line("states:");
			for (auto const& state : object.states)
				print_state(out.nested(), object, state);
		}

		void print_groups(outline const& out, gfx::loaded_model::active_object const& active) {
			auto const& object = active.object;
			if (object.groups.empty()) return;

			out.line("groups:");
			for (size_t index = 0; index < object.groups.size(); ++index) {
				auto const& [name, channels, transitions] = object.groups[index];
				auto const items = out.nested();
				items.line("- name: {}", name);

				auto const fields = items.nested();
				if (index >= active.nodes.size() || !active.nodes[index]) fields.line("node: (missing)");

				if (channels.empty()) continue;
				fields.line("channels:");
				for (size_t channel = 0; channel < channels.size(); ++channel) {
					auto const timing = channel < transitions.size() ? transitions[channel] : gfx_ia::timing{};
					fields.nested().line("- {}: {}", gfx_ia::name_of(channels[channel]), format(timing));
				}
			}
		}

		void print_slot_channels(outline const& out, loaded_object const& object) {
			if (object.slot_channels.empty()) return;

			out.line("slot channels:");
			for (auto const& [slot, transition] : object.slot_channels)
				out.nested().line("- {}: {}", object.slots[slot].name, format(transition));
		}

		void print_light_source(outline const& out,
		                        gfx::loaded_model const& model,
		                        gfx::loaded_model::active_object const& active) {
			auto const light_name = active.light ? model.lights[*active.light].name : "(missing)"s;
			out.line("light: {}", light_name);
			out.line("transition: {}", format(active.object.light));
		}

		void print_object(outline const& out,
		                  gfx::loaded_model const& model,
		                  gfx::loaded_model::active_object const& active) {
			auto const& object = active.object;
			out.line("- name: {}", object.name);

			auto const fields = out.nested();
			fields.line("kind: {}", name_of(object.kind));
			if (object.start) fields.line("start: {}", object.states[*object.start].name);

			print_slots(fields, object);
			if (object.kind == gfx_ia::object_kind::light) {
				print_light_source(fields, model, active);
				return;
			}

			print_actions(fields, object);
			print_states(fields, object);
			print_groups(fields, active);
			print_slot_channels(fields, object);
		}

		struct heading_printer {
			bool printed{};
			heading_printer() = default;
			~heading_printer() {
				if (!printed) {
					print("FOUND NOTHING INTERESTING"sv);
				}
			}

			void print(std::string_view heading) {
				if (printed) {
					std::print("\n");
				} else {
					printed = true;
				}
				std::print("{}\n", heading);
			}

			template <typename T, typename Cb>
			void section(std::string_view title, std::vector<T> const& checked, Cb&& cb) {
				if (checked.empty()) return;
				print(title);
				cb();
			}
		};
	}  // namespace

	void print_model(gfx::loaded_model const& model) {
		auto const sections = outline{};
		auto heading = heading_printer{};

		heading.section("HIERARCHY"sv, model.nodes, [&] {
			// force newlines
			print_hierarchy(sections.nested(), model, 0u);
		});
		heading.section("LIGHTS"sv, model.lights, [&] {
			// force newlines
			print_lights(sections.nested(), model);
		});
		heading.section("ACTIVE OBJECTS"sv, model.objects, [&] {
			for (auto const& active : model.objects)
				print_object(sections.nested(), model, active);
		});
	}
}  // namespace verify
