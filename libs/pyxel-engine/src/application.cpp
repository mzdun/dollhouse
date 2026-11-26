// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cstdio>
#include <exception>
#include <print>
#include <pyxel/app/application.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace pyxel::app {
	namespace {
		bool closes(event const& ev, window_id main_window) noexcept {
			return std::visit(
			    [main_window]<typename Event>(Event const& e) {
				    if constexpr (std::is_same_v<Event, quit_requested>) {
					    return true;
				    } else if constexpr (std::is_same_v<Event, window_close_requested>) {
					    return e.window == main_window;
				    } else {
					    return false;
				    }
			    },
			    ev);
		}
	}  // namespace

	window::~window() = default;
	info_source::~info_source() = default;
	platform::~platform() = default;
	renderer::~renderer() = default;
	ui::~ui() = default;
	stack::~stack() = default;

	void stack::collect_info(std::vector<info_line>& lines) {
		platform().describe(lines);
		renderer().describe(lines);
		if (auto const* layer = ui()) layer->describe(lines);
	}

	std::uint64_t stack::info_revision() {
		auto const* layer = ui();
		return platform().revision() + renderer().revision() + (layer ? layer->revision() : 0);
	}
	application::~application() = default;

	std::string application::title() const { return std::string{name()}; }

	window_options application::main_window() const { return {.title = title()}; }

	void application::set_title(std::string const& title) {
		if (window_) window_->set_title(title);
	}

	extent application::pixel_size() const noexcept { return window_ ? window_->pixel_size() : extent{}; }

	int application::main(int argc, char* argv[]) {
		try {
			if (!init(::args::from_main(argc, argv))) return 0;
			auto const backends = create_stack(main_window(), driver());
			return run(*backends);
		} catch (std::exception const& e) {
			std::println(stderr, "{}: error: {}", name(), e.what());
		}
		return 1;
	}

	int application::run(stack& backends) {
		using clock = std::chrono::steady_clock;

		auto& platform = backends.platform();
		auto& window = backends.window();
		auto& renderer = backends.renderer();
		auto* const ui = backends.ui();

		std::uint64_t info_revision{};
		auto const refresh_info = [&] {
			info_revision = backends.info_revision();
			backend_info_.clear();
			backends.collect_info(backend_info_);
		};

		// the window outlives the loop, but not necessarily the application
		struct forget_window {
			app::window*& slot;
			~forget_window() { slot = nullptr; }
		} const forget{window_};
		window_ = &window;

		quit_ = false;
		refresh_info();
		setup();

		auto last = clock::now();
		auto const frame = [&] {
			auto const now = clock::now();
			seconds const dt = now - last;
			last = now;

			if (backends.info_revision() != info_revision) refresh_info();
			update(dt);
			if (ui) {
				ui->new_frame(window, dt);
				draw_ui();
				ui->end_frame();
			}
			renderer.render();
		};

		// frames drawn from inside poll(); an exception must not go through
		// the platform's C code, so it waits for poll() to return
		std::exception_ptr live_error{};
		platform.set_live_redraw([&] {
			if (quit_ || live_error || window.minimized()) return;
			try {
				frame();
			} catch (...) {
				live_error = std::current_exception();
			}
		});
		struct forget_redraw {
			app::platform& platform;
			~forget_redraw() { platform.set_live_redraw({}); }
		} const forget_live{platform};

		std::vector<event> events{};
		while (true) {
			events.clear();
			platform.poll(events);
			if (live_error) std::rethrow_exception(live_error);
			for (auto const& ev : events) {
				if (closes(ev, window.id())) quit();
				on_event(ev);
			}
			if (quit_) break;

			if (window.minimized()) {
				platform.idle();
				last = clock::now();
				continue;
			}

			frame();
		}

		shutdown();
		renderer.wait_idle();
		return 0;
	}
}  // namespace pyxel::app
