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

		// ---- YAML scalars ---------------------------------------------------

		// a character of a model name, as shown: `_` as a space, a group
		// separator as ": "
		std::string_view shown_name(char const& c) noexcept {
			switch (c) {
				case '_':
					return " "sv;
				case '/':
				case '\\':
					return ": "sv;
				default:
					return {&c, 1};
			}
		}

		std::string_view as_is(char const& c) noexcept { return {&c, 1}; }

		bool is_control(char c) noexcept { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; }

		// the extra characters `c` needs in a double-quoted scalar
		size_t escape_length(char c) noexcept {
			if (c == '"' || c == '\\') return 1;
			if (is_control(c)) return 3;  // \xNN
			return 0;
		}

		// a plain scalar, which YAML would read as something else than a
		// string: a null, a boolean, or a number; a digit, `+` or `.` up
		// front is quoted, even if the rest is not a number
		bool reads_as_other_type(std::string_view text) noexcept {
			static constexpr std::string_view words[] = {"~"sv,  "null"sv, "true"sv, "false"sv, "yes"sv,
			                                             "no"sv, "on"sv,   "off"sv,  "y"sv,     "n"sv};
			if (text.empty()) return true;
			auto const front = text.front();
			if ((front >= '0' && front <= '9') || front == '+' || front == '.') return true;

			auto const lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
			return std::ranges::any_of(
			    words, [&](std::string_view word) { return std::ranges::equal(text, word, {}, lower); });
		}

		// `text`, after `map`, as a YAML scalar of a block mapping: plain, if
		// that reads back the same, double-quoted otherwise. One scan finds
		// the length of the result and whether it needs the quotes, so the
		// result is allocated once.
		template <typename Map>
		std::string yaml_scalar(std::string_view text, Map map) {
			// a plain scalar cannot start with any of these, nor with a space
			static constexpr std::string_view indicators = "-?:,[]{}#&*!|>'\"%@` "sv;

			auto quote = reads_as_other_type(text);
			size_t length = 0;
			size_t escapes = 0;
			char previous = '\0';
			for (auto const& c : text) {
				for (auto const out : map(c)) {
					if (length == 0 && indicators.contains(out)) quote = true;
					// a key-value separator or a comment inside the text
					if ((previous == ':' && out == ' ') || (previous == ' ' && out == '#')) quote = true;
					if (auto const extra = escape_length(out); extra) {
						quote = true;
						escapes += extra;
					}
					previous = out;
					++length;
				}
			}
			// a trailing space is lost, a trailing colon reads as a key
			if (previous == ' ' || previous == ':') quote = true;

			std::string result{};
			if (!quote) {
				result.reserve(length);
				for (auto const& c : text)
					result.append(map(c));
				return result;
			}

			result.reserve(length + escapes + 2);
			result.push_back('"');
			for (auto const& c : text) {
				for (auto const out : map(c)) {
					if (out == '"' || out == '\\') {
						result.push_back('\\');
						result.push_back(out);
					} else if (is_control(out)) {
						result.append(std::format("\\x{:02X}", static_cast<unsigned char>(out)));
					} else {
						result.push_back(out);
					}
				}
			}
			result.push_back('"');
			return result;
		}

		// ---- running the scripts ---------------------------------------------

		// runs the script, then builds its models, which the script did not
		// build itself; returns false, if the script registered no models
		bool load_and_run(script const& file, std::filesystem::path const& outdir, std::string const& exporter_id) {
			auto lib = library{};
			if (!exporter_id.empty()) {
				lib.set_exporter_id(exporter_id);
			}
			lib.set_output_directory(outdir);
			// one line per model, `    "doors: left ajar": doors/left_ajar.glb`,
			// a YAML mapping entry, with the path relative to the output
			// directory
			lib.set_export_listener([&outdir](std::string const& name, std::filesystem::path const& written) {
				auto const relative = written.lexically_relative(outdir).generic_u8string();
				auto const line = std::format("    {}: {}", yaml_scalar(name, shown_name),
				                              yaml_scalar(pyxel::as_sv(relative), as_is));
				// through sys.stdout, not std::cout: a closed pipe raises
				// BrokenPipeError here, where Python handles it, not later,
				// in the destructor of the stream redirection
				py::print(py::str{line}, "flush"_a = true);
			});

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
