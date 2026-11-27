// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "loader.hpp"

#include <exception>
#include <utility>

namespace inspector {
	using namespace std::literals;

	void loader::open(std::filesystem::path path) {
		std::lock_guard lock{path_mutex_};
		path_ = std::move(path);
	}

	void loader::reload() {
		resets_requested_.fetch_add(1, std::memory_order_relaxed);
		thread_.wake();
	}

	void loader::load(std::stop_token token) {
		std::optional<std::filesystem::path> path{};
		{
			std::lock_guard lock{path_mutex_};
			path = path_;
		}
		if (!path) return;

		loading_.store(true, std::memory_order_relaxed);
		auto const resets = resets_requested_.load(std::memory_order_relaxed);
		try {
			auto imported = pyxel::io::import_model(*path);
			// a result coming in during shutdown is dropped
			if (!token.stop_requested()) {
				observe(std::move(imported.sources), observe_mode::reset);
				results_.post(loaded{std::move(imported.model), std::move(imported.notes), resets});
			}
		} catch (pyxel::io::import_error const& error) {
			observe(error.sources(), observe_mode::append);
			results_.post(failed{error.what()});
		} catch (std::exception const& error) {
			results_.post(failed{error.what()});
		} catch (...) {
			results_.post(failed{"unknown error while loading the model"s});
		}
		loading_.store(false, std::memory_order_relaxed);
	}

	void loader::observe(std::vector<pyxel::io::source_file> /*sources*/, observe_mode /*mode*/) {}
}  // namespace inspector
