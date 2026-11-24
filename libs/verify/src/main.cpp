// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <args/parser.hpp>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <print>
#include <pyxel/base/str.hpp>
#include <pyxel/io/import.hpp>
#include <pyxel/version.hpp>
#include <span>
#include <string>
#include "checks.hpp"
#include "print.hpp"

template <>
struct args::converter<std::filesystem::path> {
	static std::filesystem::path value(args::parser&, std::string const& arg, std::string const&) {
		// std::string c-tor uses OS code page, u8string_view c-tor uses UTF-8 characters
		return std::filesystem::path{pyxel::as_u8v(arg)};
	}
};

namespace {
	struct options {
		std::filesystem::path path{};
		bool verbose{false};
		bool quiet{false};
	};

	[[noreturn]] void print_version() {
		std::print("verify {}\n", pyxel::version::ui);
		std::exit(0);
	}

	options parse_args(int argc, char* argv[]) {
		options result{};

		args::null_translator tr{};
		args::parser parser{"Verifies a model file", args::from_main(argc, argv), &tr};

		parser.custom(print_version, "version").help("show the version and exit").opt();
		parser.set<std::true_type>(result.verbose, "v", "verbose")
		    .help("report the active objects the importer upgraded, too")
		    .opt();
		parser.set<std::true_type>(result.quiet, "q", "quiet")
		    .help("report the problems only, without the model")
		    .opt();
		parser.arg(result.path).meta("<path>").help("the file to verify");
		parser.parse();

		return result;
	}

	// path: severity: active object `name`: message
	void report(std::string_view path, std::span<verify::diagnostic const> diagnostics, bool verbose) {
		for (auto const& [level, object, message] : diagnostics) {
			if (level == verify::severity::info && !verbose) continue;

			auto const where = object.empty() ? std::string{} : std::format("active object `{}`: ", object);
			std::print(std::cerr, "{}: {}: {}{}\n", path, verify::name_of(level), where, message);
		}
	}
}  // namespace

int main(int argc, char* argv[]) {
	auto const opts = parse_args(argc, argv);
	auto const path_text = opts.path.u8string();
	auto const path = pyxel::as_sv(path_text);

	try {
		auto const imported = pyxel::io::import_model(opts.path);
		auto const diagnostics = verify::check(imported);

		if (!opts.quiet) verify::print_model(imported.model);
		std::cout.flush();
		report(path, diagnostics, opts.verbose);

		auto const failed =
		    std::ranges::any_of(diagnostics, [](auto const& item) { return item.severity == verify::severity::error; });
		return failed ? EXIT_FAILURE : EXIT_SUCCESS;
	} catch (std::exception const& e) {
		std::print(std::cerr, "{}: error: {}\n", path, e.what());
		return EXIT_FAILURE;
	}
}
