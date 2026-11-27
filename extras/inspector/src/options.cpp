// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "options.hpp"

#include <cstdlib>
#include <iostream>
#include <print>
#include <pyxel/base/str.hpp>
#include <pyxel/version.hpp>
#include <string>
#include <string_view>
#include <type_traits>

ENUM_TRAITS_BEGIN(pyxel::app::gpu_api)
ENUM_TRAITS_NAME(vulkan)
ENUM_TRAITS_NAME(native)
ENUM_TRAITS_END(pyxel::app::gpu_api)

template <>
struct args::converter<std::filesystem::path> {
	static std::filesystem::path value(args::parser&, std::string const& arg, std::string const&) {
		// std::string c-tor uses OS code page, u8string_view c-tor uses UTF-8 characters
		return std::filesystem::path{pyxel::as_u8v(arg)};
	}
};

namespace inspector {
	using namespace std::literals;

	namespace {
		[[noreturn]] void print_version() {
			std::print("inspector {}\n", pyxel::version::ui);
			std::exit(1);
		}

		[[noreturn]] void file_error(std::filesystem::path const& path, std::string_view message) {
			std::print(std::cerr, "{}: error: {}\n", pyxel::as_sv(path.u8string()), message);
			std::exit(1);
		}
	}  // namespace

	options parse_options(::args::args_view const& args) {
		options result{};

		args::null_translator tr{};
		args::parser parser{"One piece of furniture, every angle", args, &tr};
		parser.custom(print_version, "version").help("show the version and exit").opt();
		parser.set<std::true_type>(result.verbose, "v").help("show more information").opt();
		parser.set<std::true_type>(result.driver.vsync, "vsync").help("turns the VSync ON").opt();
		parser.set<std::false_type>(result.driver.vsync, "no-vsync").help("turns the VSync OFF").opt();
		parser.arg(result.driver.preferred_api, "driver")
		    .meta("vulkan|native")
		    .help("selects GPU driver preference")
		    .opt();
		parser.arg(result.model_path).meta("<model>").help("inspect model from the path");

		parser.parse();

		if (auto& path = result.model_path) {
			if (!std::filesystem::exists(*path)) {
				file_error(*path, "model does not exists"sv);
			}

			if (!std::filesystem::is_regular_file(*path)) {
				file_error(*path, "model needs to be a file"sv);
			}

			path = std::filesystem::weakly_canonical(*path);
			std::print("{}\n", pyxel::as_sv(path->u8string()));
		}

		return result;
	}
}  // namespace inspector
