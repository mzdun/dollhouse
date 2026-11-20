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

	TEST_F(library_mt, second_build_is_refused_while_building) {
		add_blocking("m");
		auto builder = start_blocked_build();

		EXPECT_THROW(lib_.build_models(), std::logic_error);
		release_.count_down();
		builder.join();

		ASSERT_FALSE(error_);
		EXPECT_EQ(stems(first_), names{"m"});
	}
}  // namespace shapes::testing
