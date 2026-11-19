// Copyright (c) 2020 midnightBITS
// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <cctype>
#include <iostream>
#include <pyxel/base/str.hpp>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pyxel {
	namespace {
		template <typename String>
		struct helper;

		template <>
		struct helper<std::string_view> {
			static inline std::string_view as_str(std::string_view str) noexcept { return str; }
		};

		template <>
		struct helper<std::string> {
			static inline std::string as_str(std::string_view str) { return {str.data(), str.size()}; }
		};
	}  // namespace

#ifdef _WIN32
	// code page -> UTF-16 -> UTF-8
	std::string code_page_to_utf8(std::string_view text, unsigned code_page) {
		if (text.empty()) return {};

		auto const text_size = static_cast<int>(text.size());
		auto const wide_size = MultiByteToWideChar(code_page, 0, text.data(), text_size, nullptr, 0);
		if (wide_size <= 0) return std::string{text};
		std::wstring wide(static_cast<size_t>(wide_size), L'\0');
		MultiByteToWideChar(code_page, 0, text.data(), text_size, wide.data(), wide_size);

		auto const utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, nullptr, 0, nullptr, nullptr);
		if (utf8_size <= 0) return std::string{text};
		std::string utf8(static_cast<size_t>(utf8_size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), wide_size, utf8.data(), utf8_size, nullptr, nullptr);
		return utf8;
	}
#endif

	std::string message_u8(std::error_code const& ec) {
#ifdef _WIN32
		return code_page_to_utf8(ec.message(), CP_ACP);
#else
		return ec.message();
#endif
	}

	template <typename String>
	std::vector<String> split_impl(std::string_view sep, std::string_view data, size_t max) {
		std::vector<String> result{};

		if (sep.empty()) {
			data = strip_sv(data);

			if (data.empty()) {
				result.push_back(helper<String>::as_str(""sv));
				return result;
			}

			sep = " \f\t\v\r\n"sv;

			auto pos = data.find_first_of(sep);
			while (max && pos != std::string_view::npos) {
				auto const view = data.substr(0, pos);
				data = lstrip_sv(data.substr(pos));
				pos = data.find_first_of(sep);
				if (max != std::string::npos) --max;

				result.push_back(helper<String>::as_str(view));
			}
			result.push_back(helper<String>::as_str(lstrip_sv(data)));

			return result;
		}

		auto pos = data.find(sep);
		decltype(pos) prev = 0;

		while (max && pos != std::string_view::npos) {
			auto const view = data.substr(prev, pos - prev);
			prev = pos + sep.size();
			pos = data.find(sep, prev);
			if (max != std::string::npos) --max;

			result.push_back(helper<String>::as_str(view));
		}

		result.push_back(helper<String>::as_str(data.substr(prev)));

		return result;
	}

	size_t calc_separators(char sep, std::string_view data, size_t max) {
		auto pos = data.find(sep);
		decltype(pos) prev = 0;

		size_t result = 1;

		while (max && pos != std::string_view::npos) {
			prev = pos + 1;
			pos = data.find(sep, prev);
			if (max != std::string::npos) --max;

			++result;
		}

		return result;
	}

	template <typename String>
	std::vector<String> split_impl(char sep, std::string_view data, size_t max) {
		std::vector<String> result{};

		result.reserve(calc_separators(sep, data, max));

		auto pos = data.find(sep);
		decltype(pos) prev = 0;

		while (max && pos != std::string_view::npos) {
			auto const view = data.substr(prev, pos - prev);
			prev = pos + 1;
			pos = data.find(sep, prev);
			if (max != std::string::npos) --max;

			result.push_back(helper<String>::as_str(view));
		}

		result.push_back(helper<String>::as_str(data.substr(prev)));

		return result;
	}

	std::vector<std::string_view> split_sv(std::string_view sep, std::string& data, size_t max) {
		return split_impl<std::string_view>(sep, data, max);
	}

	std::vector<std::string_view> split_sv(std::string_view sep, std::string_view data, size_t max) {
		return split_impl<std::string_view>(sep, data, max);
	}

	std::vector<std::string> split_s(std::string_view sep, std::string_view data, size_t max) {
		return split_impl<std::string>(sep, data, max);
	}

	std::vector<std::string_view> split_sv(char sep, std::string& data, size_t max) {
		return split_impl<std::string_view>(sep, data, max);
	}

	std::vector<std::string_view> split_sv(char sep, std::string_view data, size_t max) {
		return split_impl<std::string_view>(sep, data, max);
	}

	std::vector<std::string> split_s(char sep, std::string_view data, size_t max) {
		return split_impl<std::string>(sep, data, max);
	}

	std::string_view lstrip_sv(std::string_view data) {
		auto new_stop = data.size();
		decltype(new_stop) new_start = 0;

		while (new_start < new_stop && std::isspace(static_cast<unsigned char>(data[new_start])))
			++new_start;

		return data.substr(new_start);
	}

	std::string_view rstrip_sv(std::string_view data) {
		auto new_stop = data.size();

		while (new_stop > 0 && std::isspace(static_cast<unsigned char>(data[new_stop - 1])))
			--new_stop;

		return data.substr(0, new_stop);
	}

	std::string_view strip_sv(std::string_view data) { return lstrip_sv(rstrip_sv(data)); }

	std::string strip_s(std::string_view data) {
		auto const result = strip_sv(data);
		return {result.data(), result.size()};
	}

	std::string lstrip_s(std::string_view data) {
		auto const result = lstrip_sv(data);
		return {result.data(), result.size()};
	}

	std::string rstrip_s(std::string_view data) {
		auto const result = rstrip_sv(data);
		return {result.data(), result.size()};
	}

	std::string& tolower_inplace(std::string& s) noexcept {
		for (auto& c : s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		return s;
	}

	std::string& toupper_inplace(std::string& s) noexcept {
		for (auto& c : s) {
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}

		return s;
	}

	template <typename String>
	std::string join_impl(std::string_view sep, std::vector<String> const& items) {
		std::string result{};

		if (items.empty()) return result;

		auto length = sep.size() * (items.size() - 1);
		for (auto const& item : items)
			length += item.size();

		result.reserve(length);

		auto first = true;

		for (auto const& item : items) {
			if (first)
				first = false;
			else
				result.append(sep);
			result.append(item);
		}

		return result;
	}

	std::string join(std::string_view sep, std::vector<std::string_view> const& items) { return join_impl(sep, items); }

	std::string join(std::string_view sep, std::vector<std::string> const& items) { return join_impl(sep, items); }
}  // namespace pyxel
