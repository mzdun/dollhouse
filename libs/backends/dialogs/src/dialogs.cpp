// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/dialogs.hpp>

#include <algorithm>
#include <args/parser.hpp>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cstdio>
#include <print>
#endif

namespace pyxel::dialogs {
#ifdef _WIN32
	namespace {
		std::wstring widen(std::string_view utf8) {
			if (utf8.empty()) return {};
			auto const length = static_cast<int>(utf8.size());
			auto const size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, nullptr, 0);
			std::wstring result(static_cast<size_t>(size), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, result.data(), size);
			return result;
		}

		// the edit control breaks the lines on "\r\n" only
		std::wstring for_edit(std::string_view utf8) {
			std::string text{};
			text.reserve(utf8.size() + utf8.size() / 32);
			char prev{};
			for (auto c : utf8) {
				if (c == '\n' && prev != '\r') text.push_back('\r');
				text.push_back(c);
				prev = c;
			}
			return widen(text);
		}

		// DLGTEMPLATE and its DLGITEMTEMPLATEs, built in memory; sizes are
		// in dialog units, and with a monospace font, a character is 4
		// units wide and 8 units high
		class dialog_template {
		public:
			void dialog(DWORD style,
			            WORD items,
			            short cx,
			            short cy,
			            std::wstring_view title,
			            WORD point_size,
			            std::wstring_view font) {
				dword(style);
				dword(0);  // extended style
				data_.push_back(items);
				coords(0, 0, cx, cy);
				data_.push_back(0);  // no menu
				data_.push_back(0);  // the default dialog class
				string(title);
				data_.push_back(point_size);
				string(font);
			}

			void item(DWORD style, short x, short y, short cx, short cy, WORD id, WORD atom, std::wstring_view title) {
				if (data_.size() % 2) data_.push_back(0);  // DWORD alignment
				dword(style);
				dword(0);  // extended style
				coords(x, y, cx, cy);
				data_.push_back(id);
				data_.push_back(0xFFFF);  // a predefined class follows
				data_.push_back(atom);
				string(title);
				data_.push_back(0);  // no creation data
			}

			// the vector's buffer is aligned well enough for a DLGTEMPLATE
			LPCDLGTEMPLATEW get() const noexcept { return reinterpret_cast<LPCDLGTEMPLATEW>(data_.data()); }

		private:
			void dword(DWORD value) {
				data_.push_back(LOWORD(value));
				data_.push_back(HIWORD(value));
			}

			void coords(short x, short y, short cx, short cy) {
				for (auto value : {x, y, cx, cy})
					data_.push_back(static_cast<WORD>(value));
			}

			void string(std::wstring_view text) {
				data_.insert(data_.end(), text.begin(), text.end());
				data_.push_back(0);
			}

			std::vector<WORD> data_{};
		};

		constexpr WORD button_class = 0x0080;
		constexpr WORD edit_class = 0x0081;
		constexpr WORD text_id = 100;

		INT_PTR CALLBACK text_dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam) {
			switch (msg) {
				case WM_INITDIALOG:
					SetDlgItemTextW(dlg, text_id, reinterpret_cast<wchar_t const*>(lparam));
					// the focus on the edit control would select the whole text
					SetFocus(GetDlgItem(dlg, IDOK));
					return FALSE;
				case WM_COMMAND:
					if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) {
						EndDialog(dlg, LOWORD(wparam));
						return TRUE;
					}
					break;
				default:
					break;
			}
			return FALSE;
		}

		std::string& hooked_app() {
			static std::string app{};
			return app;
		}

		void help_hook(args::parser const& parser, std::optional<size_t>) {
			show_text(hooked_app(), parser.help_text(help_width));
		}

		void error_hook(args::parser const&, std::string const& msg, std::optional<size_t>) {
			report_issue(hooked_app(), msg);
		}
	}  // namespace

	void show_text(std::string_view title, std::string_view text) {
		auto const lines = static_cast<short>(std::ranges::count(text, '\n') + 1);

		constexpr short margin = 7;
		constexpr short char_width = 4;
		constexpr short line_height = 8;
		constexpr short scroll_bar = 12;
		constexpr short max_lines = 40;
		constexpr short button_width = 50;
		constexpr short button_height = 14;

		// two more characters for the margins of the edit control
		constexpr auto text_width = static_cast<short>((help_width + 2) * char_width + scroll_bar);
		auto const text_height = static_cast<short>(std::min(lines, max_lines) * line_height + line_height / 2);
		auto const width = static_cast<short>(text_width + 2 * margin);
		auto const height = static_cast<short>(text_height + button_height + 3 * margin);

		dialog_template dlg{};
		dlg.dialog(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU, 2, width, height,
		           widen(title), 9, L"Consolas");
		dlg.item(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
		             ES_AUTOHSCROLL,
		         margin, margin, text_width, text_height, text_id, edit_class, L"");
		dlg.item(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
		         static_cast<short>(width - margin - button_width), static_cast<short>(height - margin - button_height),
		         button_width, button_height, IDOK, button_class, L"OK");

		auto const wide = for_edit(text);
		DialogBoxIndirectParamW(GetModuleHandleW(nullptr), dlg.get(), nullptr, text_dialog_proc,
		                        reinterpret_cast<LPARAM>(wide.c_str()));
	}

	void show_info(std::string_view title, std::string_view text) {
		MessageBoxW(nullptr, widen(text).c_str(), widen(title).c_str(), MB_OK | MB_ICONINFORMATION);
	}

	void report_issue(std::string_view app, std::string_view message) {
		auto const text = widen(std::format("{} reported an issue: {}", app, message));
		MessageBoxW(nullptr, text.c_str(), L"Error", MB_OK | MB_ICONERROR);
	}

	args_hooks::args_hooks(std::string app) : help_{args::set_help(help_hook)}, error_{args::set_error(error_hook)} {
		hooked_app() = std::move(app);
	}

	args_hooks::~args_hooks() {
		args::set_help(help_);
		args::set_error(error_);
	}
#else
	void show_text(std::string_view, std::string_view text) { std::print("{}", text); }

	void show_info(std::string_view, std::string_view text) { std::println("{}", text); }

	void report_issue(std::string_view app, std::string_view message) {
		std::println(stderr, "{}: error: {}", app, message);
	}

	args_hooks::args_hooks(std::string) {}

	args_hooks::~args_hooks() = default;
#endif
}  // namespace pyxel::dialogs
