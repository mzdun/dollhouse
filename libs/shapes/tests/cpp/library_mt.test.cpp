// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <exception>
#include <format>
#include <latch>
#include <pyxel/gfx/solid_object.hpp>
#include <set>
#include <shapes/library.hpp>
#include <thread>

// The Python module is free-threading ready (py::mod_gil_not_used), so
// scripts may register and build models on one library from many threads.
// These tests pin down how the registry (add_model, build_models,
// clear_models, close), the configuration (exporter, output directory,
// export listener) and the paints are expected to interact then.

namespace shapes::testing {
	namespace fs = std::filesystem;
	using names = std::vector<std::string>;

	class library_mt : public ::testing::Test {
	protected:
		void SetUp() override {
			dir_ = fs::temp_directory_path() /
			       std::format("shapes-mt-{}", ::testing::UnitTest::GetInstance()->current_test_info()->name());
			fs::remove_all(dir_);
			lib_.set_output_directory(dir_);
		}
		void TearDown() override { fs::remove_all(dir_); }

		static model_fn empty() {
			return [](pyxel::gfx::group&) -> std::optional<std::string> { return std::nullopt; };
		}

		// a unit cube painted with `paint`; unlike empty(), it makes the
		// export look the paint up
		static void add_cube(pyxel::gfx::group& root, std::string paint) {
			using namespace pyxel::math::literals;
			root.set_material(std::move(paint));
			root.add<pyxel::gfx::solid_object>(pyxel::gfx::solid::cube(1_m, 1_m, 1_m));
		}

		static model_fn cube(std::string paint) {
			return [paint = std::move(paint)](pyxel::gfx::group& root) -> std::optional<std::string> {
				add_cube(root, paint);
				return std::nullopt;
			};
		}

		// registers `name`, which signals `entered_` and waits for `release_`
		void add_blocking(std::string name) {
			lib_.add_model(std::move(name), [this](pyxel::gfx::group&) -> std::optional<std::string> {
				entered_.count_down();
				release_.wait();
				return std::nullopt;
			});
		}

		// runs build_models() on another thread; returns, once that thread
		// is inside the blocking model
		std::jthread start_blocked_build() {
			std::jthread builder{[this] {
				try {
					first_ = lib_.build_models();
				} catch (...) {
					error_ = std::current_exception();
				}
			}};
			entered_.wait();
			return builder;
		}

		static names stems(std::vector<fs::path> const& paths) {
			names result{};
			result.reserve(paths.size());
			for (auto const& path : paths)
				result.push_back(path.stem().string());
			std::ranges::sort(result);
			return result;
		}

		library lib_{};
		fs::path dir_{};
		std::latch entered_{1};
		std::latch release_{1};
		std::vector<fs::path> first_{};
		std::exception_ptr error_{};
	};

	// a model added while building is left for the next build, wherever its
	// name sorts relative to the model being built
	TEST_F(library_mt, model_added_mid_build_waits_for_next_build) {
		add_blocking("m");
		auto builder = start_blocked_build();

		EXPECT_TRUE(lib_.add_model("a_late", empty()));  // sorts before the running model
		EXPECT_TRUE(lib_.add_model("z_late", empty()));  // sorts after it
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		EXPECT_EQ(stems(first_), names{"m"});
		EXPECT_EQ(stems(lib_.build_models()), (names{"a_late", "z_late"}));
	}

	TEST_F(library_mt, second_build_is_refused_while_building) {
		add_blocking("m");
		auto builder = start_blocked_build();

		EXPECT_THROW(lib_.build_models(), std::logic_error);
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		EXPECT_EQ(stems(first_), names{"m"});
	}

	TEST_F(library_mt, clear_is_refused_while_building) {
		add_blocking("m");
		lib_.add_model("n", empty());  // the builder has to step onto this one after "m"
		auto builder = start_blocked_build();

		EXPECT_THROW(lib_.clear_models(), std::logic_error);
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		EXPECT_EQ(stems(first_), (names{"m", "n"}));
	}

	// the host goes away while another thread builds: new models are refused
	// at once, the registered ones are forgotten after the build
	TEST_F(library_mt, close_while_building_forgets_the_models_after_the_build) {
		add_blocking("m");
		lib_.add_model("n", empty());
		auto builder = start_blocked_build();

		lib_.close();
		EXPECT_TRUE(lib_.is_closed());
		EXPECT_THROW(lib_.add_model("late", empty()), std::logic_error);
		EXPECT_EQ(lib_.models().size(), 2u);  // still in use by the build
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		EXPECT_EQ(stems(first_), (names{"m", "n"}));
		EXPECT_TRUE(lib_.models().empty());
	}

	// no synchronization between the threads; meant to be run under TSan
	TEST_F(library_mt, stress_add_while_building) {
		constexpr int adders = 4;
		constexpr int per_adder = 50;
		std::atomic<int> adders_left{adders};
		std::vector<fs::path> built{};

		std::jthread builder{[&] {
			while (adders_left.load() > 0) {
				auto paths = lib_.build_models();
				built.insert(built.end(), paths.begin(), paths.end());
			}
		}};
		{
			std::vector<std::jthread> threads{};
			threads.reserve(adders);
			for (int i = 0; i < adders; ++i) {
				threads.emplace_back([&, i] {
					for (int j = 0; j < per_adder; ++j) {
						auto const name = std::format("t{}_{:03}", i, j);
						EXPECT_TRUE(lib_.add_model(name, empty()));
						EXPECT_FALSE(lib_.add_model(name, empty()));  // duplicates are still rejected
					}
					adders_left.fetch_sub(1);
				});
			}
		}  // joins the adders
		builder.join();

		auto const rest = lib_.build_models();
		built.insert(built.end(), rest.begin(), rest.end());

		auto const all = stems(built);
		EXPECT_EQ(all.size(), std::size_t{adders * per_adder});
		EXPECT_EQ(std::set(all.begin(), all.end()).size(), all.size());  // each written exactly once
	}

	TEST_F(library_mt, configuration_is_refused_while_building) {
		add_blocking("m");
		auto builder = start_blocked_build();

		// the getters do not wait for the build
		auto const exporter = lib_.exporter_id();
		EXPECT_EQ(lib_.output_directory(), dir_);

		EXPECT_THROW(lib_.set_exporter_id("obj"), std::logic_error);
		EXPECT_THROW(lib_.set_output_directory(dir_ / "elsewhere"), std::logic_error);
		EXPECT_THROW(lib_.set_export_listener({}), std::logic_error);
		EXPECT_EQ(lib_.exporter_id(), exporter);
		EXPECT_EQ(lib_.output_directory(), dir_);
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		ASSERT_EQ(first_.size(), 1u);
		EXPECT_EQ(first_.front().parent_path(), dir_);

		// allowed again, once the build is over
		EXPECT_NO_THROW(lib_.set_exporter_id("obj"));
		EXPECT_NO_THROW(lib_.set_output_directory(dir_ / "elsewhere"));
	}

	TEST_F(library_mt, configuration_is_refused_from_a_model) {
		lib_.add_model("m", [this](pyxel::gfx::group&) -> std::optional<std::string> {
			EXPECT_THROW(lib_.set_exporter_id("obj"), std::logic_error);
			EXPECT_THROW(lib_.set_output_directory(dir_ / "elsewhere"), std::logic_error);
			EXPECT_THROW(lib_.set_export_listener({}), std::logic_error);
			return std::nullopt;
		});

		auto const paths = lib_.build_models();
		ASSERT_EQ(paths.size(), 1u);
		EXPECT_EQ(paths.front().parent_path(), dir_);
		EXPECT_NE(paths.front().extension(), ".obj");
	}

	// the paints are not locked while the model runs, only while it exports
	TEST_F(library_mt, model_registers_its_own_paint) {
		lib_.add_model("m", [this](pyxel::gfx::group& root) -> std::optional<std::string> {
			add_cube(root, lib_.register_paint("wood", {.color = pyxel::gfx::rgb(0x80, 0x50, 0x20)}));
			return std::nullopt;
		});

		EXPECT_EQ(stems(lib_.build_models()), names{"m"});
	}

	// no synchronization between the threads; meant to be run under TSan
	TEST_F(library_mt, stress_register_paint_while_building) {
		constexpr std::size_t painters = 4;
		constexpr std::size_t per_painter = 50;
		constexpr int models = 20;

		auto const base = lib_.register_paint("base", {.color = pyxel::gfx::rgb(0x80, 0x80, 0x80)});
		for (int i = 0; i < models; ++i)
			lib_.add_model(std::format("m{:02}", i), cube(base));

		std::latch start{static_cast<std::ptrdiff_t>(painters) + 1};
		std::vector<fs::path> built{};
		std::vector<names> registered(painters);
		{
			std::jthread builder{[&] {
				start.arrive_and_wait();
				built = lib_.build_models();
			}};
			std::vector<std::jthread> threads{};
			threads.reserve(painters);
			for (std::size_t i = 0; i < painters; ++i) {
				threads.emplace_back([&, i] {
					start.arrive_and_wait();
					for (std::size_t j = 0; j < per_painter; ++j) {
						auto const color = pyxel::gfx::rgb(static_cast<uint8_t>(i), static_cast<uint8_t>(j), 0);
						registered[i].push_back(lib_.register_paint(std::format("p{}_{:03}", i, j), {.color = color}));
					}
				});
			}
		}  // joins everyone

		EXPECT_EQ(built.size(), std::size_t{models});
		for (std::size_t i = 0; i < painters; ++i) {
			ASSERT_EQ(registered[i].size(), per_painter);
			for (std::size_t j = 0; j < per_painter; ++j)
				EXPECT_EQ(registered[i][j], std::format("p{}_{:03}", i, j));
		}
	}

	// no synchronization between the threads; meant to be run under TSan
	TEST_F(library_mt, stress_getters_while_setting) {
		constexpr int readers = 4;
		constexpr int rounds = 200;

		std::string const exporters[] = {lib_.exporter_id(), "obj"};
		fs::path const dirs[] = {dir_ / "even", dir_ / "odd"};
		std::atomic<bool> done{false};

		std::vector<std::jthread> threads{};
		threads.reserve(readers + 1);
		for (int i = 0; i < readers; ++i) {
			threads.emplace_back([&] {
				while (!done.load()) {
					auto const exporter = lib_.exporter_id();
					auto const dir = lib_.output_directory();
					EXPECT_TRUE(exporter == exporters[0] || exporter == exporters[1]) << exporter;
					EXPECT_TRUE(dir == dir_ || dir == dirs[0] || dir == dirs[1]) << dir;
				}
			});
		}
		threads.emplace_back([&] {
			for (int i = 0; i < rounds; ++i) {
				lib_.set_exporter_id(exporters[i % 2]);
				lib_.set_output_directory(dirs[i % 2]);
			}
			done.store(true);
		});
	}
	// the listener is called on the building thread, with every model written
	TEST_F(library_mt, export_listener_sees_every_model) {
		lib_.add_model("a", empty());
		lib_.add_model("b", empty());
		std::vector<std::pair<std::string, fs::path>> seen{};
		auto const builder_id = std::this_thread::get_id();
		lib_.set_export_listener([&](std::string const& name, fs::path const& path) {
			EXPECT_EQ(std::this_thread::get_id(), builder_id);
			EXPECT_FALSE(lib_.is_closed());  // no lock is held during the call
			seen.emplace_back(name, path);
		});

		auto const paths = lib_.build_models();
		ASSERT_EQ(seen.size(), 2u);
		EXPECT_EQ(seen[0], (std::pair<std::string, fs::path>{"a", paths[0]}));
		EXPECT_EQ(seen[1], (std::pair<std::string, fs::path>{"b", paths[1]}));
	}
}  // namespace shapes::testing
