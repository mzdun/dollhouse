// Copyright (c) 2021 midnightBITS
// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace std::literals;

namespace pyxel {
	template <typename Result, typename InputView>
	inline Result conv(InputView view) {
		return Result{reinterpret_cast<typename Result::value_type const*>(view.data()), view.size()};
	}

	inline auto as_u8v(std::string_view view) { return conv<std::u8string_view>(view); }
	inline auto as_sv(std::u8string_view view) { return conv<std::string_view>(view); }
	inline auto as_str(std::u8string_view view) { return conv<std::string>(view); }

	// ec.message() as UTF-8; on Windows, the message comes in the ANSI code
	// page and is translated, elsewhere it is returned as is
	std::string message_u8(std::error_code const& ec);

#ifdef _WIN32
	// text in the given code page (e.g. CP_ACP, 1250) as UTF-8; if Windows
	// cannot convert it, the text is returned as is
	std::string code_page_to_utf8(std::string_view text, unsigned code_page);
#endif

	std::vector<std::string_view> split_sv(std::string_view sep, std::string& data, size_t max = std::string::npos);
	std::vector<std::string_view> split_sv(std::string_view sep, std::string_view data, size_t max = std::string::npos);
	std::vector<std::string> split_s(std::string_view sep, std::string_view data, size_t max = std::string::npos);
	std::vector<std::string_view> split_sv(char sep, std::string& data, size_t max = std::string::npos);
	std::vector<std::string_view> split_sv(char sep, std::string_view data, size_t max = std::string::npos);
	std::vector<std::string> split_s(char sep, std::string_view data, size_t max = std::string::npos);

	std::string_view strip_sv(std::string_view);
	std::string_view lstrip_sv(std::string_view);
	std::string_view rstrip_sv(std::string_view);

	std::string strip_s(std::string_view);
	std::string lstrip_s(std::string_view);
	std::string rstrip_s(std::string_view);

	std::string& tolower_inplace(std::string&) noexcept;
	std::string& toupper_inplace(std::string&) noexcept;

	std::string join(std::string_view sep, std::vector<std::string_view> const& items);
	std::string join(std::string_view sep, std::vector<std::string> const& items);
}  // namespace pyxel
