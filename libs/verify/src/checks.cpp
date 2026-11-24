// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "checks.hpp"
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

namespace verify {
	namespace {
		namespace gfx = pyxel::gfx;
		namespace gfx_ia = pyxel::gfx::interactions;
		namespace io_ia = pyxel::io::interactions;

		class collector {
		public:
			explicit collector(std::vector<diagnostic>& out, std::string object = {})
			    : out_{out}, object_{std::move(object)} {}

			template <typename... Args>
			void report(severity level, std::format_string<Args...> fmt, Args&&... args) {
				out_.push_back({
				    .severity = level,
				    .object = object_,
				    .message = std::format(fmt, std::forward<Args>(args)...),
				});
			}

		private:
			std::vector<diagnostic>& out_;
			std::string object_;
		};

		void check_notes(std::vector<diagnostic>& out, std::vector<io_ia::note> const& notes) {
			for (auto const& [object, what, reason] : notes) {
				auto const [level, action] = what == io_ia::note::dropped ? std::pair{severity::error, "left out"sv}
				                             : what == io_ia::note::ignored
				                                 ? std::pair{severity::warning, "read in part"sv}
				                                 : std::pair{severity::info, "upgraded"sv};
				collector{out, object}.report(level, "{}: {}", action, reason);
			}
		}

		// The states reachable from the start, by any edge of the state
		// machine; an object without states reaches none.
		std::vector<bool> reachable_states(gfx_ia::loaded_object const& object) {
			std::vector<bool> reached(object.states.size(), false);
			if (!object.start) return reached;

			std::vector<size_t> pending{*object.start};
			reached[*object.start] = true;
			while (!pending.empty()) {
				auto const current = pending.back();
				pending.pop_back();
				for (auto const& [action, next] : object.states[current].machine) {
					if (reached[next]) continue;
					reached[next] = true;
					pending.push_back(next);
				}
			}
			return reached;
		}

		void check_state_machine(collector& report, gfx_ia::loaded_object const& object) {
			auto const reached = reachable_states(object);

			std::vector<bool> offered(object.actions.size(), false);
			for (size_t index = 0; index < object.states.size(); ++index) {
				auto const& state = object.states[index];
				if (!reached[index]) {
					report.report(severity::warning, "state `{}` cannot be reached from `{}`", state.name,
					              object.states[*object.start].name);
					continue;
				}
				for (auto const& [action, next] : state.machine) {
					if (action != gfx_ia::default_interaction) offered[static_cast<size_t>(action)] = true;
				}
			}

			for (size_t index = 0; index < object.actions.size(); ++index) {
				if (!offered[index])
					report.report(severity::warning, "action \"{}\" is never offered", object.actions[index]);
			}
		}

		void check_resolved(collector& report, gfx::loaded_model::active_object const& active) {
			auto const& object = active.object;
			if (object.kind == gfx_ia::object_kind::light) {
				if (!active.light) report.report(severity::error, "describes no light of the model");
				return;
			}

			for (size_t index = 0; index < object.groups.size(); ++index) {
				if (index < active.nodes.size() && active.nodes[index]) continue;
				report.report(severity::error, "moves group `{}`, which is not a node of the model",
				              object.groups[index].group);
			}
		}
	}  // namespace

	std::vector<diagnostic> check(pyxel::io::imported_model const& imported) {
		std::vector<diagnostic> result{};
		check_notes(result, imported.notes);

		for (auto const& active : imported.model.objects) {
			collector report{result, active.object.name};
			check_resolved(report, active);
			check_state_machine(report, active.object);
		}

		return result;
	}

	std::string_view name_of(severity level) noexcept {
		switch (level) {
			case severity::error:
				return "error"sv;
			case severity::warning:
				return "warning"sv;
			case severity::info:
				break;
		}
		return "info"sv;
	}
}  // namespace verify
