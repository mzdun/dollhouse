// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "panels/log.hpp"

#include <imgui.h>
#include <format>
#include <pyxel/io/interactions.hpp>
#include <string_view>

#include "application.hpp"

namespace inspector::panels {
	using namespace std::literals;

	namespace {
		std::string_view describe(pyxel::io::interactions::note::kind what) noexcept {
			switch (what) {
				case pyxel::io::interactions::note::upgraded:
					return "upgraded"sv;
				case pyxel::io::interactions::note::ignored:
					return "ignored"sv;
				case pyxel::io::interactions::note::dropped:
					return "dropped"sv;
			}
			return "note"sv;
		}
	}  // namespace

	void log::draw(application const& app) {
		if (ImGui::Begin("Log")) {
			ImGui::TextUnformatted(header.c_str());
			ImGui::Text("%.1f FPS", fps.observe(static_cast<double>(ImGui::GetIO().Framerate)));
			if (app.loading()) ImGui::TextUnformatted("Loading...");
			if (auto const& error = app.error(); !error.empty())
				ImGui::TextColored(ImVec4{1.0f, 0.4f, 0.4f, 1.0f}, "%s", error.c_str());
			for (auto const& note : app.notes()) {
				auto const line = std::format("[Import] {}: {}: {}", note.object.empty() ? "<unnamed>"sv : note.object,
				                              describe(note.what), note.reason);
				ImGui::TextUnformatted(line.c_str());
			}
			if (app.verbose()) {
				for (auto const& [source, label, value] : app.backend_info())
					ImGui::Text("[%s] %s: %s", source.c_str(), label.c_str(), value.c_str());
				auto const& camera = app.camera();
				ImGui::TextUnformatted(
				    std::format("[Camera] eye: {:.3f}, {:.3f}, {:.3f}", camera.eye.x, camera.eye.y, camera.eye.z)
				        .c_str());
				ImGui::TextUnformatted(std::format("[Camera] target: {:.3f}, {:.3f}, {:.3f}", camera.target.x,
				                                   camera.target.y, camera.target.z)
				                           .c_str());
				ImGui::TextUnformatted(
				    std::format("[Camera] near/far: {:.3f} / {:.3f}", camera.near_plane, camera.far_plane).c_str());
			}
		}
		ImGui::End();
	}
}  // namespace inspector::panels
