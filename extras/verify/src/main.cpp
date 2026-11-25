// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <algorithm>
#include <args/parser.hpp>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <pyxel/base/str.hpp>
#include <pyxel/io/import.hpp>
#include <pyxel/io/pose.hpp>
#include <pyxel/version.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "checks.hpp"
#include "emulate.hpp"
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
		std::vector<std::string> emulate{};
		std::optional<std::filesystem::path> write{};
		bool verbose{false};
		bool quiet{false};
	};

	// `-e=value`, written as the long options are: the parser gives the
	// short ones everything after the letter, the `=` included
	std::string without_equals(std::string const& value) { return value.starts_with('=') ? value.substr(1) : value; }

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
		parser.arg(result.emulate, "e", "emulate")
		    .meta("'<action> <object>'")
		    .help(
		        "take the action of the active object, starting from its starting state; repeat for more "
		        "actions, taken in order; the object is the last word, or the last \"quoted name\", and the rest "
		        "is the action, \"<click>\" for the default interaction")
		    .opt();
		parser.arg(result.write, "w", "write")
		    .meta("<path>")
		    .help(
		        "write a copy of the model, posed by the emulated actions, without its active objects; the "
		        "model is verified, but not printed")
		    .opt();
		parser.arg(result.path).meta("<path>").help("the file to verify");
		parser.parse();

		for (auto& value : result.emulate)
			value = without_equals(value);
		if (result.write)
			result.write =
			    std::filesystem::path{pyxel::as_u8v(without_equals(pyxel::as_str(result.write->u8string())))};

		return result;
	}

	// takes every --emulate action, printing the steps, unless `quiet`;
	// returns the states reached, or the error of the first action failing
	std::optional<std::vector<std::optional<size_t>>> emulate(std::string_view path,
	                                                          pyxel::gfx::loaded_model const& model,
	                                                          std::span<std::string const> values,
	                                                          bool quiet) {
		verify::emulator runner{model};
		std::vector<verify::emulated_step> steps{};
		std::optional<std::string> failure{};
		for (auto const& value : values) {
			try {
				steps.push_back(runner.take(verify::parse_emulated(value)));
			} catch (verify::emulation_error const& error) {
				failure = std::format("{}: error: emulate '{}': {}", path, value, error.what());
				break;
			}
		}

		if (!quiet && !steps.empty()) {
			std::print("EMULATION\n");
			for (auto const& [object, action, from, to] : steps)
				std::print("  - {} {}: {} -> {}\n", action, object, from, to);
		}
		std::cout.flush();

		if (failure) {
			std::print(std::cerr, "{}\n", *failure);
			return std::nullopt;
		}
		auto const states = runner.states();
		return std::vector<std::optional<size_t>>{states.begin(), states.end()};
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

		// the copy is the output of a run with --write
		if (!opts.quiet && !opts.write) {
			verify::print_model(imported.model);
			if (!opts.emulate.empty()) std::print("\n");
		}
		auto const states = emulate(path, imported.model, opts.emulate, opts.quiet);
		report(path, diagnostics, opts.verbose);

		auto const failed =
		    std::ranges::any_of(diagnostics, [](auto const& item) { return item.severity == verify::severity::error; });
		if (failed || !states) return EXIT_FAILURE;

		if (opts.write) pyxel::io::write_posed(opts.path, *opts.write, imported.model, imported.model.moves(*states));
		return EXIT_SUCCESS;
	} catch (std::exception const& e) {
		std::print(std::cerr, "{}: error: {}\n", path, e.what());
		return EXIT_FAILURE;
	}
}
