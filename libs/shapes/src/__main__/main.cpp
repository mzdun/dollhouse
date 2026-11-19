// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <filesystem>
#include <format>
#include <iostream>
#include <pyxel/base/str.hpp>
#include <pyxel/version.hpp>
#include <shapes/__main__/main.hpp>
#include <shapes/glue/glue.hpp>
#include <shapes/library.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "config.hpp"

namespace shapes::__main__ {
	namespace {
		// UTF-8, as Python expects in sys.path and in file names
		py::str as_py(std::filesystem::path const& path) { return py::str{pyxel::as_str(path.u8string())}; }

		// the directory of the script goes to the end of sys.path, so the
		// scripts can import their helpers, but no helper shadows a module
		// of the standard library; removed again, once the script is done
		class search_path {
		public:
			explicit search_path(py::str dir)
			    : path_{py::module_::import("sys").attr("path").cast<py::list>()}, dir_{std::move(dir)} {
				path_.append(dir_);
			}

			search_path(search_path const&) = delete;
			search_path& operator=(search_path const&) = delete;

			~search_path() {
				// the last entry equal to ours, in case the script added
				// more after it, or the same one earlier
				try {
					for (auto index = py::len(path_); index > 0; --index) {
						if (py::object{path_[index - 1]}.equal(dir_)) {
							path_.attr("pop")(index - 1);
							break;
						}
					}
				} catch (py::error_already_set&) {
					// nothing to restore; the run is ending with an error
				}
			}

		private:
			py::list path_;
			py::str dir_;
		};

		// runs the script, then builds its models, which the script did not
		// build itself; returns false, if the script registered no models
		bool load_and_run(script const& file, std::filesystem::path const& outdir, std::string const& exporter_id) {
			auto lib = library{};
			if (!exporter_id.empty()) {
				lib.set_exporter_id(exporter_id);
			}
			lib.set_output_directory(outdir);

			auto const path = std::filesystem::absolute(file.path);
			search_path const guard{as_py(path.parent_path())};

			// loaded from its file, under a name, which no import statement
			// can spell, so it is neither shadowed by a module already loaded
			// (e.g. a script named `abc`), nor imported again by a helper
			auto const name = std::format("shapes:{}", file.name);
			auto const util = py::module_::import("importlib.util");
			auto const spec = util.attr("spec_from_file_location")(name, as_py(path));
			auto const module = util.attr("module_from_spec")(spec);
			py::module_::import("sys").attr("modules")[py::str{name}] = module;
			spec.attr("loader").attr("exec_module")(module);

			lib.build_models();
			return !lib.models().empty();
		}
	}  // namespace

	int main(std::vector<std::string>& py_argv) {
		auto cfg = config::from(py_argv);
		if (cfg.return_code >= 0) {
			return cfg.return_code;
		}

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

		for (auto const& file : cfg.scripts) {
			if (!load_and_run(file, outdir, cfg.exporter_id)) {
				auto const path = file.path.u8string();
				std::cerr << std::format("python -m shapes: error: script `{}` registered no models\n",
				                         pyxel::as_sv(path));
				return 1;
			}
		}
		return 0;
	}
}  // namespace shapes::__main__
