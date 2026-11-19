// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cerrno>
#include <pyxel/base/str.hpp>
#include <string>
#include <string_view>
#include <system_error>
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

namespace pyxel::testing {
	namespace {
		using strings = std::vector<std::string>;
		using views = std::vector<std::string_view>;

		// "Zażółć gęślą jaźń", the Polish pangram
		constexpr auto polish_utf8 = u8"Zażółć gęślą jaźń"sv;
		// the same text in windows-1250
		constexpr auto polish_1250 = "Za\xBF\xF3\xB3\xE6 g\xEA\x9Cl\xB9 ja\x9F\xF1"sv;

		// error category with a fixed message, for any encoding the test needs
		class fixed_category : public std::error_category {
		public:
			explicit fixed_category(std::string_view message) : message_{message} {}
			char const* name() const noexcept override { return "pyxel-test"; }
			std::string message(int) const override { return std::string{message_}; }

		private:
			std::string_view message_;
		};

		bool is_inside(std::string_view view, std::string const& storage) {
			auto const* begin = storage.data();
			auto const* end = begin + storage.size();
			return view.data() >= begin && view.data() + view.size() <= end;
		}
	}  // namespace

	// ---- conv / as_u8v / as_sv / as_str ----------------------------------

	TEST(str_conv, as_u8v_views_the_same_bytes) {
		std::string const text{"Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87"};
		auto const view = as_u8v(text);
		EXPECT_EQ(static_cast<void const*>(view.data()), static_cast<void const*>(text.data()));
		EXPECT_EQ(as_sv(view), as_sv(u8"Zażółć"sv));
	}

	TEST(str_conv, as_sv_views_the_same_bytes) {
		std::u8string const text{polish_utf8};
		auto const view = as_sv(text);
		EXPECT_EQ(static_cast<void const*>(view.data()), static_cast<void const*>(text.data()));
		EXPECT_EQ(view.size(), text.size());
		EXPECT_TRUE(as_u8v(view) == polish_utf8);
	}

	TEST(str_conv, as_str_copies_the_bytes) {
		std::u8string const text{polish_utf8};
		auto const copy = as_str(text);
		EXPECT_NE(static_cast<void const*>(copy.data()), static_cast<void const*>(text.data()));
		EXPECT_EQ(copy, as_sv(polish_utf8));
	}

	TEST(str_conv, keeps_embedded_nul_and_empty) {
		EXPECT_EQ(as_u8v("a\0b"sv).size(), 3u);
		EXPECT_EQ(as_sv(u8"a\0b"sv), "a\0b"sv);
		EXPECT_EQ(as_str(u8""sv), ""s);
		EXPECT_TRUE(as_u8v(""sv).empty());
	}

	// ---- message_u8 --------------------------------------------------------

	TEST(str_message_u8, default_error_code) {
		std::error_code const ec{};
		EXPECT_FALSE(message_u8(ec).empty());
	}

	TEST(str_message_u8, ascii_comes_out_unchanged) {
		fixed_category const category{"plain ASCII message"};
		EXPECT_EQ(message_u8(std::error_code{1, category}), "plain ASCII message"s);
	}

	TEST(str_message_u8, empty_comes_out_empty) {
		fixed_category const category{""};
		EXPECT_EQ(message_u8(std::error_code{1, category}), ""s);
	}

#ifndef _WIN32
	// outside Windows, the messages are already UTF-8: what comes in, comes out

	TEST(str_message_u8, utf8_comes_out_unchanged) {
		auto const utf8 = as_sv(polish_utf8);
		fixed_category const category{utf8};
		EXPECT_EQ(message_u8(std::error_code{1, category}), utf8);
	}

	TEST(str_message_u8, other_bytes_come_out_unchanged) {
		fixed_category const category{polish_1250};
		EXPECT_EQ(message_u8(std::error_code{1, category}), polish_1250);
	}

	TEST(str_message_u8, system_and_generic_categories) {
		for (auto const ec :
		     {std::error_code{ENOENT, std::system_category()}, std::error_code{ENOENT, std::generic_category()},
		      std::make_error_code(std::errc::permission_denied)}) {
			EXPECT_EQ(message_u8(ec), ec.message()) << ec.category().name() << ':' << ec.value();
		}
	}
#endif

#ifdef _WIN32
	namespace {
		bool is_valid_utf8(std::string_view text) {
			if (text.empty()) return true;
			return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
			                           nullptr, 0) > 0;
		}
	}  // namespace

	// ---- code_page_to_utf8 -------------------------------------------------

	TEST(str_code_page_to_utf8, windows_1250_says_it_in_polish) {
		EXPECT_EQ(code_page_to_utf8(polish_1250, 1250), as_sv(polish_utf8));
	}

	TEST(str_code_page_to_utf8, the_code_page_matters) {
		// the same bytes read as windows-1252
		EXPECT_EQ(code_page_to_utf8(polish_1250, 1252), as_sv(u8"Za¿ó³æ gêœl¹ jaŸñ"sv));
	}

	TEST(str_code_page_to_utf8, utf8_code_page_is_unchanged) {
		EXPECT_EQ(code_page_to_utf8(as_sv(polish_utf8), CP_UTF8), as_sv(polish_utf8));
	}

	TEST(str_code_page_to_utf8, empty_is_empty) { EXPECT_EQ(code_page_to_utf8(""sv, 1250), ""s); }

	TEST(str_code_page_to_utf8, unknown_code_page_returns_the_text) {
		// MultiByteToWideChar fails with ERROR_INVALID_PARAMETER
		EXPECT_EQ(code_page_to_utf8(polish_1250, 12345), polish_1250);
	}

	TEST(str_code_page_to_utf8, keeps_embedded_nul) {
		EXPECT_EQ(code_page_to_utf8("a\0\xB9"sv, 1250), as_sv(u8"a\0ą"sv));
	}

	// ---- message_u8, Windows -------------------------------------------------

	TEST(str_message_u8, ansi_message_is_translated) {
		// the ANSI code page is set system-wide (or by the manifest); a Polish
		// system says windows-1250, others decode the same bytes their way
		fixed_category const category{polish_1250};
		auto const message = message_u8(std::error_code{1, category});
		EXPECT_EQ(message, code_page_to_utf8(polish_1250, CP_ACP));
		EXPECT_TRUE(is_valid_utf8(message));
		if (GetACP() == 1250) EXPECT_EQ(message, as_sv(polish_utf8));
	}

	TEST(str_message_u8, system_messages_are_utf8) {
		for (auto const ec : {std::error_code{ERROR_FILE_NOT_FOUND, std::system_category()},
		                      std::error_code{ERROR_ACCESS_DENIED, std::system_category()},
		                      std::make_error_code(std::errc::permission_denied)}) {
			auto const message = message_u8(ec);
			EXPECT_FALSE(message.empty()) << ec.category().name() << ':' << ec.value();
			EXPECT_TRUE(is_valid_utf8(message)) << ec.category().name() << ':' << ec.value();
		}
	}
#endif

	// ---- split, string separator ---------------------------------------------

	TEST(str_split, string_separator) {
		EXPECT_EQ(split_s("::"sv, "a::b::::c"sv), (strings{"a", "b", "", "c"}));
		EXPECT_EQ(split_sv("::"sv, "a::b::::c"sv), (views{"a", "b", "", "c"}));
	}

	TEST(str_split, string_separator_at_the_edges) {
		EXPECT_EQ(split_s("::"sv, "::a::"sv), (strings{"", "a", ""}));
		EXPECT_EQ(split_s("::"sv, "a"sv), (strings{"a"}));
		EXPECT_EQ(split_s("::"sv, ""sv), (strings{""}));
		EXPECT_EQ(split_s("::"sv, ":"sv), (strings{":"}));
	}

	TEST(str_split, string_separator_with_max) {
		EXPECT_EQ(split_s("::"sv, "a::b::c::d"sv, 0), (strings{"a::b::c::d"}));
		EXPECT_EQ(split_s("::"sv, "a::b::c::d"sv, 1), (strings{"a", "b::c::d"}));
		EXPECT_EQ(split_s("::"sv, "a::b::c::d"sv, 2), (strings{"a", "b", "c::d"}));
		EXPECT_EQ(split_s("::"sv, "a::b"sv, 5), (strings{"a", "b"}));
	}

	TEST(str_split, empty_separator_splits_on_whitespace) {
		EXPECT_EQ(split_s(""sv, "  a  b\tc\r\n d \f\v"sv), (strings{"a", "b", "c", "d"}));
		EXPECT_EQ(split_sv(""sv, "a"sv), (views{"a"}));
		EXPECT_EQ(split_sv(""sv, " a \t b\nc "sv), (views{"a", "b", "c"}));
		EXPECT_EQ(split_sv(""sv, " a \t b\nc "sv, 1), (views{"a", "b\nc"}));
	}

	TEST(str_split, empty_separator_on_blank_data) {
		EXPECT_EQ(split_s(""sv, ""sv), (strings{""}));
		EXPECT_EQ(split_s(""sv, " \t\r\n"sv), (strings{""}));
		EXPECT_EQ(split_sv(""sv, " \t\r\n"sv), (views{""}));
	}

	TEST(str_split, empty_separator_with_max) {
		EXPECT_EQ(split_s(""sv, " a  b \t c "sv, 0), (strings{"a  b \t c"}));
		EXPECT_EQ(split_s(""sv, " a  b \t c "sv, 1), (strings{"a", "b \t c"}));
		EXPECT_EQ(split_s(""sv, " a  b \t c "sv, 2), (strings{"a", "b", "c"}));
		EXPECT_EQ(split_s(""sv, " a  b \t c "sv, 9), (strings{"a", "b", "c"}));
	}

	TEST(str_split, string_separator_views_point_into_the_data) {
		std::string data{"one, two, three"};
		auto const result = split_sv(", "sv, data);
		ASSERT_EQ(result, (views{"one", "two", "three"}));
		for (auto const view : result)
			EXPECT_TRUE(is_inside(view, data)) << view;
	}

	// ---- split, char separator -------------------------------------------------

	TEST(str_split, char_separator) {
		EXPECT_EQ(split_s(',', "a,b,,c"sv), (strings{"a", "b", "", "c"}));
		EXPECT_EQ(split_sv(',', "a,b,,c"sv), (views{"a", "b", "", "c"}));
	}

	TEST(str_split, char_separator_at_the_edges) {
		EXPECT_EQ(split_s(',', ",a,"sv), (strings{"", "a", ""}));
		EXPECT_EQ(split_s(',', "a"sv), (strings{"a"}));
		EXPECT_EQ(split_s(',', ""sv), (strings{""}));
		EXPECT_EQ(split_s(',', ","sv), (strings{"", ""}));
	}

	TEST(str_split, char_separator_with_max) {
		EXPECT_EQ(split_s(',', "a,b,c,d"sv, 0), (strings{"a,b,c,d"}));
		EXPECT_EQ(split_s(',', "a,b,c,d"sv, 1), (strings{"a", "b,c,d"}));
		EXPECT_EQ(split_s(',', "a,b,c,d"sv, 2), (strings{"a", "b", "c,d"}));
		EXPECT_EQ(split_s(',', "a,b"sv, 5), (strings{"a", "b"}));
	}

	TEST(str_split, char_separator_does_not_split_on_whitespace) {
		EXPECT_EQ(split_s(' ', " a  b "sv), (strings{"", "a", "", "b", ""}));
	}

	TEST(str_split, char_separator_views_point_into_the_data) {
		std::string data{"one/two/three"};
		auto const result = split_sv('/', data);
		ASSERT_EQ(result, (views{"one", "two", "three"}));
		for (auto const view : result)
			EXPECT_TRUE(is_inside(view, data)) << view;
	}

	TEST(str_split, utf8_is_split_by_bytes) {
		auto const text = as_sv(u8"ą/ę"sv);
		EXPECT_EQ(split_s('/', text), (strings{std::string{as_sv(u8"ą"sv)}, std::string{as_sv(u8"ę"sv)}}));
	}

	// ---- strip ---------------------------------------------------------------------

	TEST(str_strip, views) {
		auto const text = " \t\r\n a b \f\v\n"sv;
		EXPECT_EQ(lstrip_sv(text), "a b \f\v\n"sv);
		EXPECT_EQ(rstrip_sv(text), " \t\r\n a b"sv);
		EXPECT_EQ(strip_sv(text), "a b"sv);
	}

	TEST(str_strip, copies) {
		auto const text = "  a b  "sv;
		EXPECT_EQ(lstrip_s(text), "a b  "s);
		EXPECT_EQ(rstrip_s(text), "  a b"s);
		EXPECT_EQ(strip_s(text), "a b"s);
	}

	TEST(str_strip, views_point_into_the_text) {
		std::string const text{"  a b  "};
		EXPECT_TRUE(is_inside(lstrip_sv(text), text));
		EXPECT_TRUE(is_inside(rstrip_sv(text), text));
		EXPECT_TRUE(is_inside(strip_sv(text), text));
	}

	TEST(str_strip, blank_and_empty) {
		for (auto const text : {""sv, " "sv, " \t\r\n\f\v"sv}) {
			EXPECT_EQ(lstrip_sv(text), ""sv);
			EXPECT_EQ(rstrip_sv(text), ""sv);
			EXPECT_EQ(strip_sv(text), ""sv);
			EXPECT_EQ(strip_s(text), ""s);
		}
	}

	TEST(str_strip, nothing_to_strip) {
		EXPECT_EQ(strip_sv("a b"sv), "a b"sv);
		EXPECT_EQ(lstrip_sv("a "sv), "a "sv);
		EXPECT_EQ(rstrip_sv(" a"sv), " a"sv);
	}

	TEST(str_strip, keeps_non_ascii_bytes) {
		// bytes >= 0x80 go to isspace as unsigned char, and are not spaces in "C"
		auto const nbsp = as_sv(u8" "sv);
		auto const text = std::string{" "} + std::string{nbsp} + "x" + std::string{nbsp} + " ";
		EXPECT_EQ(strip_s(text), std::string{nbsp} + "x" + std::string{nbsp});
		EXPECT_EQ(strip_sv(polish_1250), polish_1250);
	}

	// ---- tolower / toupper -----------------------------------------------------------

	TEST(str_case, tolower_inplace) {
		std::string text{"MiXeD 123 _-"};
		auto& result = tolower_inplace(text);
		EXPECT_EQ(&result, &text);
		EXPECT_EQ(text, "mixed 123 _-"s);
	}

	TEST(str_case, toupper_inplace) {
		std::string text{"MiXeD 123 _-"};
		auto& result = toupper_inplace(text);
		EXPECT_EQ(&result, &text);
		EXPECT_EQ(text, "MIXED 123 _-"s);
	}

	TEST(str_case, keeps_non_ascii_bytes) {
		auto const utf8 = std::string{as_sv(polish_utf8)};
		auto lower = utf8;
		auto upper = utf8;
		tolower_inplace(lower);
		toupper_inplace(upper);
		EXPECT_EQ(lower, as_sv(u8"zażółć gęślą jaźń"sv));
		EXPECT_EQ(upper, as_sv(u8"ZAżółć GęśLą JAźń"sv));
	}

	TEST(str_case, empty) {
		std::string text{};
		EXPECT_EQ(tolower_inplace(text), ""s);
		EXPECT_EQ(toupper_inplace(text), ""s);
	}

	// ---- join ------------------------------------------------------------------------

	TEST(str_join, views) {
		EXPECT_EQ(join(", "sv, views{}), ""s);
		EXPECT_EQ(join(", "sv, views{"a"}), "a"s);
		EXPECT_EQ(join(", "sv, views{"a", "b", "c"}), "a, b, c"s);
		EXPECT_EQ(join(", "sv, views{"", ""}), ", "s);
	}

	TEST(str_join, strings) {
		EXPECT_EQ(join("/"sv, strings{}), ""s);
		EXPECT_EQ(join("/"sv, strings{"a"}), "a"s);
		EXPECT_EQ(join("/"sv, strings{"a", "b", "c"}), "a/b/c"s);
		EXPECT_EQ(join(""sv, strings{"a", "b", "c"}), "abc"s);
	}

	TEST(str_join, reverses_split) {
		auto const text = "a::b::::c::"sv;
		EXPECT_EQ(join("::"sv, split_sv("::"sv, text)), text);
		EXPECT_EQ(join(","sv, split_s(',', "x,,y,"sv)), "x,,y,"s);
	}
}  // namespace pyxel::testing
