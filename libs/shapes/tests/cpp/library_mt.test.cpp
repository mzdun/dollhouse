// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <exception>
#include <format>
#include <latch>
#include <set>
#include <shapes/library.hpp>
#include <thread>

// The Python module is free-threading ready (py::mod_gil_not_used), so
// scripts may register and build models on one library from many threads.
// These tests pin down how add_model, build_models and clear_models are
// expected to interact then.

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
}  // namespace shapes::testing
