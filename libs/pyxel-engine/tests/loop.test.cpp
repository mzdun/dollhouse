// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <fakes.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pyxel::app::testing {
	using ::testing::ElementsAre;
	using ::testing::IsEmpty;

	struct test_app : application {
		journal& log;
		std::function<void(test_app&)> on_update{};
		std::function<std::unique_ptr<stack>()> make_stack{};
		bool init_result{true};
		std::optional<driver_hints> hints{};
		driver_hints received_hints{.vsync = false, .preferred_api = gpu_api::native};

		explicit test_app(journal& log) : log{log} {}

		std::string_view name() const noexcept override { return "test-app"; }
		bool init(::args::args_view const&) override {
			log.push_back("init");
			return init_result;
		}
		driver_hints driver() const override { return hints ? *hints : application::driver(); }
		std::unique_ptr<stack> create_stack(window_options const& options, driver_hints const& driver) override {
			log.push_back("create_stack(" + options.title + ")");
			received_hints = driver;
			if (!make_stack) throw std::runtime_error{"no backends"};
			return make_stack();
		}
		void setup() override { log.push_back("setup"); }
		void on_event(event const& ev) override {
			log.push_back(std::holds_alternative<quit_requested>(ev) ? "event(quit)" : "event(close)");
		}
		void update(seconds) override {
			log.push_back("update");
			if (on_update) on_update(*this);
		}
		void draw_ui() override { log.push_back("draw_ui"); }
		void shutdown() override { log.push_back("shutdown"); }
	};

	TEST(loop, quits_before_first_frame) {
		journal log{};
		fake_stack backends{log};
		test_app app{log};

		EXPECT_EQ(0, app.run(backends));
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "event(quit)", "shutdown", "wait_idle"));
	}

	TEST(loop, frame_order) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{}};
		test_app app{log};

		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update", "new_frame", "draw_ui", "end_frame",
		                             "render", "poll", "event(quit)", "shutdown", "wait_idle"));
	}

	TEST(loop, no_ui) {
		journal log{};
		fake_stack backends{log};
		backends.with_ui = false;
		backends.platform_.script = {{}};
		test_app app{log};

		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update", "render", "poll", "event(quit)", "shutdown",
		                             "wait_idle"));
	}

	TEST(loop, closing_other_window_goes_on) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{window_close_requested{.window = 99}},
		                             {window_close_requested{.window = backends.window_.id_}}};
		test_app app{log};
		app.on_update = [](test_app& self) { self.log.push_back("(frame)"); };

		app.run(backends);
		EXPECT_THAT(
		    log, ElementsAre("describe", "setup", "poll", "event(close)", "update", "(frame)", "new_frame", "draw_ui",
		                     "end_frame", "render", "poll", "event(close)", "shutdown", "wait_idle"));
	}

	TEST(loop, minimized_idles) {
		journal log{};
		fake_stack backends{log};
		backends.window_.minimized_ = true;
		backends.platform_.script = {{}, {}};
		test_app app{log};

		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "idle", "poll", "idle", "poll", "event(quit)",
		                             "shutdown", "wait_idle"));
	}

	TEST(loop, quit_from_update_finishes_frame) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{}, {}, {}};
		test_app app{log};
		app.on_update = [](test_app& self) { self.quit(); };

		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update", "new_frame", "draw_ui", "end_frame",
		                             "render", "poll", "shutdown", "wait_idle"));
	}

	TEST(loop, live_redraw_draws_inside_poll) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{}};
		backends.platform_.live_redraws = 1;
		test_app app{log};

		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update", "new_frame", "draw_ui", "end_frame",
		                             "render", "update", "new_frame", "draw_ui", "end_frame", "render", "poll",
		                             "event(quit)", "shutdown", "wait_idle"));
		EXPECT_FALSE(backends.platform_.redraw);
	}

	TEST(loop, live_redraw_skipped_while_minimized) {
		journal log{};
		fake_stack backends{log};
		backends.window_.minimized_ = true;
		backends.platform_.script = {{}};
		backends.platform_.live_redraws = 1;
		test_app app{log};

		app.run(backends);
		EXPECT_THAT(log,
		            ElementsAre("describe", "setup", "poll", "idle", "poll", "event(quit)", "shutdown", "wait_idle"));
	}

	TEST(loop, live_redraw_error_leaves_poll) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{}};
		backends.platform_.live_redraws = 2;
		test_app app{log};
		app.on_update = [](test_app&) { throw std::runtime_error{"update failed"}; };

		EXPECT_THROW(app.run(backends), std::runtime_error);
		// the second live frame is not tried, and the event batch of that
		// poll never reaches the application
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update"));
		EXPECT_FALSE(backends.platform_.redraw);
	}

	TEST(loop, can_run_again) {
		journal log{};
		fake_stack backends{log};
		test_app app{log};
		app.on_update = [](test_app& self) { self.quit(); };

		backends.platform_.script = {{}, {}};
		app.run(backends);
		log.clear();
		backends.platform_.script = {{}, {}};
		app.run(backends);
		EXPECT_THAT(log, ElementsAre("describe", "setup", "poll", "update", "new_frame", "draw_ui", "end_frame",
		                             "render", "poll", "shutdown", "wait_idle"));
	}

	TEST(loop, backend_info_is_cached) {
		journal log{};
		fake_stack backends{log};
		backends.renderer_.info = {{.source = "GPU", .label = "Driver", .value = "fake"}};
		backends.platform_.script = {{}, {}, {}};
		test_app app{log};
		int frame{};
		app.on_update = [&](test_app& self) {
			++frame;
			if (frame == 1) {
				EXPECT_EQ(2u, self.backend_info().size());
				backends.renderer_.info = {{.source = "GPU", .label = "Driver", .value = "other"}};
				backends.renderer_.info_revision = 1;
			} else if (frame >= 2) {
				ASSERT_EQ(2u, self.backend_info().size());
				EXPECT_EQ((info_line{.source = "GPU", .label = "Driver", .value = "other"}), self.backend_info()[0]);
				EXPECT_EQ((info_line{.source = "UI", .label = "Name", .value = "fake"}), self.backend_info()[1]);
			}
		};

		app.run(backends);
		EXPECT_EQ(3, frame);
		EXPECT_EQ(2, std::ranges::count(log, "describe"));
	}

	TEST(loop, set_title_reaches_window_while_running) {
		journal log{};
		fake_stack backends{log};
		backends.platform_.script = {{}};
		test_app app{log};

		app.set_title("before");
		EXPECT_THAT(backends.window_.title_, IsEmpty());

		app.on_update = [](test_app& self) { self.set_title("during"); };
		app.run(backends);
		EXPECT_EQ("during", backends.window_.title_);

		app.set_title("after");
		EXPECT_EQ("during", backends.window_.title_);
	}

	TEST(main, init_hands_over) {
		journal log{};
		test_app app{log};
		app.init_result = false;

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(0, app.main(1, argv));
		EXPECT_THAT(log, ElementsAre("init"));
	}

	TEST(main, failing_backends) {
		journal log{};
		test_app app{log};

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(1, app.main(1, argv));
		EXPECT_THAT(log, ElementsAre("init", "create_stack(test-app)"));
	}

	TEST(main, failure_reaches_report_error) {
		struct reporting_app : test_app {
			using test_app::test_app;
			void report_error(std::string_view message) override {
				log.push_back("report_error(" + std::string{message} + ")");
			}
		};

		journal log{};
		reporting_app app{log};

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(1, app.main(1, argv));
		EXPECT_THAT(log, ElementsAre("init", "create_stack(test-app)", "report_error(no backends)"));
	}

	TEST(main, default_driver_hints) {
		journal log{};
		test_app plain{log};

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(1, plain.main(1, argv));
		EXPECT_EQ((driver_hints{}), plain.received_hints);
		EXPECT_TRUE(plain.received_hints.vsync);
		EXPECT_EQ(gpu_api::vulkan, plain.received_hints.preferred_api);
	}

	TEST(main, driver_hints_reach_the_stack) {
		journal log{};
		test_app app{log};
		app.hints = {.vsync = false, .preferred_api = gpu_api::native};

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(1, app.main(1, argv));
		EXPECT_EQ(*app.hints, app.received_hints);
	}

	TEST(main, runs_the_loop) {
		journal log{};
		test_app app{log};
		app.make_stack = [&log] { return std::make_unique<fake_stack>(log); };

		char arg0[] = "test-app";
		char* argv[] = {arg0, nullptr};
		EXPECT_EQ(0, app.main(1, argv));
		EXPECT_THAT(log, ElementsAre("init", "create_stack(test-app)", "describe", "setup", "poll", "event(quit)",
		                             "shutdown", "wait_idle"));
	}

	TEST(main, default_window) {
		journal log{};
		test_app app{log};
		auto const options = app.main_window();
		EXPECT_EQ("test-app", options.title);
		EXPECT_EQ((extent{1280, 800}), options.size);
		EXPECT_THAT(log, IsEmpty());
	}
}  // namespace pyxel::app::testing
