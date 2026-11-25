// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "config.hpp"
#include <algorithm>
#include <args/parser.hpp>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <pyxel/base/str.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/version.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

template <>
struct args::converter<std::filesystem::path> {
	static std::filesystem::path value(args::parser&, std::string const& arg, std::string const&) {
		// std::string c-tor uses OS code page, u8string_view c-tor uses UTF-8 characters
		return std::filesystem::path{pyxel::as_u8v(arg)};
	}
};

namespace shapes::__main__ {
	namespace {
		using pyxel::io::export_formats;

		// owns a copy of the arguments, with the char* view args::from_main
		// expects; must outlive the parser made from it
		class arg_stg {
		public:
			explicit arg_stg(std::vector<std::string>& argv) : stg_{std::move(argv)} {
				items_.reserve(stg_.size() + 1);
				for (auto& item : stg_)
					items_.push_back(item.data());
				items_.push_back(nullptr);
			}

			arg_stg(arg_stg const&) = delete;
			arg_stg& operator=(arg_stg const&) = delete;

			operator ::args::args_view() { return ::args::from_main(static_cast<int>(stg_.size()), items_.data()); }

		private:
			std::vector<std::string> stg_;
			std::vector<char*> items_{};
		};

		// thrown by the actions ending the program early, e.g. --formats;
		// unlike std::exit, it lets Python shut down cleanly
		struct exit_request {
			int code{};
		};

		// the same message as parser.error(), but written to std::cerr (so it
		// follows sys.stderr) and ending with exit_request, not std::exit
		[[noreturn]] void usage_error(args::parser const& parser, std::string const& msg) {
			auto usage = parser.tr()(args::lng::usage);
			parser.printer_append_usage(usage);
			std::cerr << usage << '\n' << parser.tr()(args::lng::error_msg, parser.program(), msg) << '\n';
			throw exit_request{2};
		}

		// UTF-8, whatever the code page; path::string() would use the ANSI
		// one on Windows, and may throw
		std::string as_text(std::filesystem::path const& path) { return pyxel::as_str(path.u8string()); }

		constexpr std::u8string_view model_suffix = u8".model.py";

		// `chair` is `chair.model.py`, or `chair.py`; `chair.py` and
		// `chair.model.py` name the file directly
		std::optional<script> find_script(std::filesystem::path const& dir, std::string const& name) {
			auto const u8name = pyxel::as_u8v(name);
			std::vector<std::u8string> candidates{};
			if (u8name.ends_with(u8".py"sv)) {
				candidates.emplace_back(u8name);
			} else {
				candidates.push_back(std::u8string{u8name} + std::u8string{model_suffix});
				candidates.push_back(std::u8string{u8name} + u8".py");
			}

			std::error_code ec{};
			for (auto const& filename : candidates) {
				auto path = dir / filename;
				if (!std::filesystem::is_regular_file(path, ec)) continue;

				auto stem = std::u8string_view{filename};
				stem.remove_suffix(stem.ends_with(model_suffix) ? model_suffix.size() : u8".py"sv.size());
				return script{.path = std::move(path), .name = pyxel::as_str(stem)};
			}
			return std::nullopt;
		}

		bool is_known_format(std::string_view id) {
			return std::ranges::any_of(export_formats(), [id](auto const& format) { return format.id == id; });
		}

		[[noreturn]] void print_version() {
			std::cout << std::format("shapes {}\n", pyxel::version::ui);
			throw exit_request{0};
		}

		// one row per exporter known to Assimp: id, file extension and the
		// description, if Assimp has one
		[[noreturn]] void print_formats() {
			auto const& formats = export_formats();

			size_t id_width = 0;
			size_t ext_width = 0;
			for (auto const& format : formats) {
				id_width = (std::max)(id_width, format.id.size());
				ext_width = (std::max)(ext_width, format.extension.size() + 1);
			}

			std::cout << "known formats:\n";
			for (auto const& format : formats) {
				auto const ext = "." + format.extension;
				auto line = std::format("  {:<{}}  {:<{}}", format.id, id_width, ext, ext_width);
				if (!format.description.empty()) line += "  " + format.description;
				while (!line.empty() && line.back() == ' ')
					line.pop_back();
				std::cout << line << '\n';
			}
			throw exit_request{0};
		}
	}  // namespace

	config config::from(std::vector<std::string>& py_argv) {
		config result{};

		try {
			auto argv = arg_stg{py_argv};

			args::null_translator tr{};
			args::parser parser{"", argv, &tr};
			// under `-m`, argv[0] is the path of the extension
			parser.program("python -m shapes");

			parser.custom(print_version, "version").help("show the version and exit").opt();
			parser.arg(result.input, "d")
			    .meta("<dir>")
			    .help("directory containing the scripts; defaults to current directory")
			    .opt();
			parser.arg(result.outdir, "o")
			    .meta("<dir>")
			    .help("directory to write the model files to; defaults to current directory")
			    .opt();
			parser.arg(result.exporter_id, "f", "format")
			    .meta("<id>")
			    .help(
			        "model file format to use for output, see --formats to list all known formats; defaults to `glb2` "
			        "or `glb`, whichever is available")
			    .opt();
			parser.custom(print_formats, "formats").help("list all known model file formats and exit").opt();
#if SHAPES_HAS_WATCH
			parser.set<std::true_type>(result.watch, "w", "watch")
			    .help(
			        "keep running and re-export the model(s) whenever a script changes; please note, at this point the "
			        "script imports are not being tracked")
			    .opt();
#endif  // SHAPES_HAS_WATCH
			std::vector<std::string> names{};
			parser.arg(names)
			    .meta("<script>")
			    .help(
			        "script(s) to run, by name: `chair` runs `chair.model.py`, or `chair.py`, if there is no "
			        "`chair.model.py`; defaults to all `*.model.py` scripts")
			    .opt();
			parser.parse();

			if (!result.exporter_id.empty() && !is_known_format(result.exporter_id)) {
				usage_error(parser,
				            std::format("unknown format `{}`; see --formats for the known ones", result.exporter_id));
			}

			std::error_code ec{};
			if (!result.input.empty() && !std::filesystem::is_directory(result.input, ec)) {
				auto const what = std::filesystem::exists(result.input, ec) ? "is not a directory" : "does not exist";
				usage_error(parser, std::format("input directory `{}` {}", as_text(result.input), what));
			}

			// the output directory is created later, if missing
			if (!result.outdir.empty() && std::filesystem::exists(result.outdir, ec) &&
			    !std::filesystem::is_directory(result.outdir, ec)) {
				usage_error(parser, std::format("output path `{}` is not a directory", as_text(result.outdir)));
			}

			// an empty path means the current directory, but the iterator
			// would not accept it
			auto const dir = result.input.empty() ? std::filesystem::path{"."} : result.input;

			if (!names.empty()) {
				for (auto const& name : names) {
					auto found = find_script(dir, name);
					if (!found) {
						usage_error(parser, std::format("script `{}` not found in `{}`", name, as_text(dir)));
					}
					result.scripts.push_back(std::move(*found));
				}
			} else {
				std::filesystem::directory_iterator it{dir, ec};
				if (ec) {
					usage_error(parser, std::format("cannot list the scripts in `{}`: {}", as_text(dir),
					                                pyxel::message_u8(ec)));
				}

				// only the model scripts; other .py files are helpers, which
				// the scripts import, and are never run on their own
				for (auto const& entry : it) {
					if (!entry.is_regular_file(ec)) continue;
					auto const filename = entry.path().filename().u8string();
					if (!filename.ends_with(model_suffix)) continue;
					auto const name = std::u8string_view{filename}.substr(0, filename.size() - model_suffix.size());
					if (name.empty()) continue;

					result.scripts.push_back({.path = entry.path(), .name = pyxel::as_str(name)});
				}
				if (result.scripts.empty()) {
					usage_error(parser, std::format("no `*.model.py` scripts in `{}`", as_text(dir)));
				}

				// the directory order is unspecified
				std::ranges::sort(result.scripts, {}, &script::name);
			}
		} catch (exit_request const& request) {
			std::cout.flush();
			result.return_code = request.code;
		}

		return result;
	}
}  // namespace shapes::__main__
