// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <args/sys.hpp>
#include <cstddef>
#include <string>
#include <string_view>

// What an application has to say before it has a window of its own. A WIN32
// application on Windows has no console to print to, so there it shows a
// dialog box; elsewhere it prints to stdout or stderr, as a terminal
// application would.
namespace pyxel::dialogs {
	// the width of the help text, in characters; the help dialog shows it
	// in a monospace font
	inline constexpr std::size_t help_width = 100;

	// Windows: a read-only, monospace text box with an OK button; elsewhere:
	// the text on stdout
	void show_text(std::string_view title, std::string_view text);
	// Windows: an information message box; elsewhere: the text on stdout
	void show_info(std::string_view title, std::string_view text);
	// Windows: an "Error" message box, saying "<app> reported an issue:
	// <message>"; elsewhere: "<app>: error: <message>" on stderr
	void report_issue(std::string_view app, std::string_view message);

	// Windows: while alive, sends the help of every args::parser to
	// show_text() and its errors to report_issue(); elsewhere, the parser
	// prints them, as it always did. args::exit() still follows either.
	// The hooks of mbits-args are global, so only one of these should be
	// alive at a time; the destructor puts the previous hooks back.
	class args_hooks {
	public:
		explicit args_hooks(std::string app);
		~args_hooks();
		args_hooks(args_hooks const&) = delete;
		args_hooks& operator=(args_hooks const&) = delete;

	private:
		args::help_function help_{};
		args::error_function error_{};
	};
}  // namespace pyxel::dialogs
