// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <args/parser.hpp>
#include <memory>
#include <pyxel/app/backends.hpp>
#include <pyxel/app/driver.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::app {
	struct application {
		virtual ~application();

		// init(), create_stack(), run(); returns the exit code
		int main(int argc, char* argv[]);
		// the main loop, over the backends given
		int run(stack& backends);

		virtual std::string_view name() const noexcept = 0;
		virtual std::string title() const;
		virtual window_options main_window() const;
		// asked after init(), so command line switches can land here
		virtual driver_hints driver() const { return {}; }

		// true: go on to the loop; false: done already (e.g. --help);
		// an exception: stop with an error
		virtual bool init(::args::args_view const&) { return true; }
		// the application picks its backends here
		virtual std::unique_ptr<stack> create_stack(window_options const& options, driver_hints const& hints) = 0;

		virtual void setup() {}
		virtual void on_event(event const&) {}
		virtual void update(seconds) {}
		virtual void draw_ui() {}
		virtual void shutdown() {}

		// leaves the loop before the next frame
		void quit() noexcept { quit_ = true; }

		// what the backends of the running loop tell about themselves,
		// collected again only when they change
		std::vector<info_line> const& backend_info() const noexcept { return backend_info_; }

		// window API; reaches the window only while run() is going (from
		// setup() to shutdown()), does nothing otherwise
		void set_title(std::string const& title);
		// in pixels; empty without a window
		extent pixel_size() const noexcept;

	private:
		window* window_{nullptr};
		bool quit_{false};
		std::vector<info_line> backend_info_{};
	};
}  // namespace pyxel::app
