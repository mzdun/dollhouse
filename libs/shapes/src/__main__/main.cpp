// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>
#include <pyxel/base/str.hpp>
#include <pyxel/version.hpp>
#include <shapes/__main__/main.hpp>
#include <shapes/glue/glue.hpp>
#include <shapes/library.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "config.hpp"

namespace shapes::__main__ {
	namespace {
		void load_and_run(std::string const& name,
		                  std::filesystem::path const& outdir,
		                  std::string const& exporter_id) {
			auto lib = library{};
			if (!exporter_id.empty()) {
				lib.set_exporter_id(exporter_id);
			}
			lib.set_output_directory(outdir);

			py::module_::import(name.c_str());

			// build any models not yet build by the script itself
			lib.build_models();
		}

		void run_scripts(std::span<std::string const> module_names,
		                 std::filesystem::path const& outdir,
		                 std::string const& exporter_id) {
			for (auto const& filename : module_names) {
				load_and_run(filename, outdir, exporter_id);
			}
		}
	}  // namespace

	int main(std::vector<std::string>& py_argv) {
		auto cfg = config::from(py_argv);
		if (cfg.return_code >= 0) {
			return cfg.return_code;
		}

		// the scripts are imported as modules, so their directory goes
		// first on the search path; absolute, so a later chdir does not
		// change it, and as UTF-8 text, as Python expects in sys.path
		auto const sys = py::module_::import("sys");
		auto const input_dir =
		    cfg.input.empty() ? std::filesystem::current_path() : std::filesystem::absolute(cfg.input);
		sys.attr("path").attr("insert")(0, py::str{pyxel::as_str(input_dir.u8string())});

		// mkdir -p; an empty path is the current directory, which exists
		auto const outdir =
		    cfg.outdir.empty() ? std::filesystem::current_path() : std::filesystem::absolute(cfg.outdir);
		std::error_code ec{};
		std::filesystem::create_directories(outdir, ec);
		if (ec) {
			auto const outdir_utf8 = outdir.u8string();
			std::cerr << std::format("python -m shapes: error: cannot create output directory `{}`: {}\n",
			                         pyxel::as_sv(outdir_utf8), pyxel::message_u8(ec));
			return 1;
		}

		run_scripts(cfg.modules, outdir, cfg.exporter_id);
		return 0;
	}
}  // namespace shapes::__main__
