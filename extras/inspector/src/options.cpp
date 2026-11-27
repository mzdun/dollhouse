// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "options.hpp"

#include <args/sys.hpp>
#include <format>
#include <print>
#include <pyxel/base/str.hpp>
#include <pyxel/dialogs.hpp>
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

	options parse_options(::args::args_view const& args, std::string_view app) {
		options result{};

		// the help and the errors below reach the user even without a
		// console (see pyxel/dialogs.hpp)
		pyxel::dialogs::args_hooks hooks{std::string{app}};

		args::null_translator tr{};
		args::parser parser{"One piece of furniture, every angle", args, &tr};
		parser
		    .custom(
		        [app] {
			        pyxel::dialogs::show_info(app, std::format("inspector {}", pyxel::version::ui));
			        args::exit(0);
		        },
		        "version")
		    .help("show the version and exit")
		    .opt();
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
			auto const name = pyxel::as_str(path->u8string());
			if (!std::filesystem::exists(*path)) {
				parser.error(std::format("{}: the model does not exist", name));
			}

			if (!std::filesystem::is_regular_file(*path)) {
				parser.error(std::format("{}: the model needs to be a file", name));
			}

			path = std::filesystem::weakly_canonical(*path);
			std::print("{}\n", pyxel::as_sv(path->u8string()));
		}

		return result;
	}
}  // namespace inspector
