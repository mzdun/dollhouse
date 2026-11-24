// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "emulate.hpp"
#include <algorithm>
#include <format>
#include <string>
#include <utility>

using namespace std::literals;

namespace verify {
	namespace {
		namespace gfx_ia = pyxel::gfx::interactions;

		std::string_view trim(std::string_view text) noexcept {
			auto const first = text.find_first_not_of(" \t\r\n"sv);
			if (first == std::string_view::npos) return {};
			auto const last = text.find_last_not_of(" \t\r\n"sv);
			return text.substr(first, last - first + 1);
		}

		std::string_view unquote(std::string_view text) noexcept {
			if (text.size() >= 2 && text.front() == '"' && text.back() == '"') return text.substr(1, text.size() - 2);
			return text;
		}
	}  // namespace

	emulated_action parse_emulated(std::string_view value) {
		auto const text = trim(value);

		std::string_view action{};
		std::string_view object{};
		if (text.ends_with('"')) {
			auto const open = text.size() < 2 ? std::string_view::npos : text.rfind('"', text.size() - 2);
			if (open == std::string_view::npos) throw emulation_error{"a quote is not closed"};
			object = text.substr(open + 1, text.size() - open - 2);
			action = text.substr(0, open);
		} else {
			auto const split = text.find_last_of(" \t\r\n"sv);
			object = split == std::string_view::npos ? text : text.substr(split + 1);
			action = split == std::string_view::npos ? std::string_view{} : text.substr(0, split);
			// `"left rocker`, or `"left rocker" now`
			if (object.find('"') != std::string_view::npos) throw emulation_error{"a quote is not closed"};
		}

		// quotes inside the action, not around it, are a part of its label
		action = unquote(trim(action));

		if (action.empty() || object.empty()) throw emulation_error{"expected an action and an active object"};
		return {.action = std::string{action}, .object = std::string{object}};
	}

	emulator::emulator(pyxel::gfx::loaded_model const& model) : model_{model}, states_{model.start_states()} {}

	emulated_step emulator::take(emulated_action const& request) {
		auto const& objects = model_.objects;
		auto const found = std::ranges::find(
		    objects, request.object, [](auto const& active) -> std::string const& { return active.object.name; });
		if (found == objects.end()) throw emulation_error{std::format("no active object `{}`", request.object)};

		auto const index = static_cast<size_t>(found - objects.begin());
		auto const& object = found->object;
		auto const current = states_[index];
		if (!current || *current >= object.states.size())
			throw emulation_error{std::format("active object `{}` has no states", object.name)};
		auto const& state = object.states[*current];

		std::ptrdiff_t action = gfx_ia::default_interaction;
		auto label = std::string{click};
		if (request.action != click) {
			auto const it = std::ranges::find(object.actions, request.action);
			if (it == object.actions.end()) {
				throw emulation_error{
				    std::format("active object `{}` has no action \"{}\"", object.name, request.action)};
			}
			action = it - object.actions.begin();
			label = std::format("\"{}\"", request.action);
		}

		auto const edge = state.machine.find(action);
		if (edge == state.machine.end()) {
			throw emulation_error{
			    std::format("active object `{}`: {} does nothing in state `{}`", object.name, label, state.name)};
		}

		states_[index] = edge->second;
		return {
		    .object = object.name,
		    .action = std::move(label),
		    .from = state.name,
		    .to = object.states[edge->second].name,
		};
	}
}  // namespace verify
