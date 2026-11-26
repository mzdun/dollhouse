// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <concepts>
#include <memory>
#include <pyxel/app/application.hpp>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
// minwindef.h defines both as empty macros, which breaks pyxel::gfx::near
#undef near
#undef far

namespace pyxel::app {
	struct arg_stg {
		std::vector<std::string> utf8;
		std::vector<char*> args{args_for_utf8(utf8)};

		static std::vector<std::string> wide_char_to_utf8(int argc, wchar_t* argv[]) {
			std::vector<std::string> result{};
			result.resize(argc);
			std::transform(argv, argv + argc, result.begin(), ut8f_str);
			return result;
		}

		static std::vector<std::string> wide_char_to_utf8() {
			int argc = 0;
			auto const argv_ptr = local_ptr<LPWSTR>{CommandLineToArgvW(GetCommandLineW(), &argc)};
			auto argv = argv_ptr.get();

			return wide_char_to_utf8(argc, argv);
		}

		static std::string ut8f_str(wchar_t const* arg) {
			if (!arg) return {};

			auto size = WideCharToMultiByte(CP_UTF8, 0, arg, -1, nullptr, 0, nullptr, nullptr);
			auto out = std::make_unique<char[]>(size + 1);
			WideCharToMultiByte(CP_UTF8, 0, arg, -1, out.get(), size + 1, nullptr, nullptr);
			return out.get();
		}

		template <typename T>
		struct local_free {
			void operator()(T* ptr) { LocalFree(ptr); }
		};

		template <typename T>
		using local_ptr = std::unique_ptr<T, local_free<T>>;

		static std::vector<char*> args_for_utf8(std::vector<std::string>& memory) {
			std::vector<char*> result{};
			result.resize(memory.size() + 1u);
			std::transform(memory.begin(), memory.end(), result.begin(), [](auto& s) { return s.data(); });
			result[memory.size()] = nullptr;
			return result;
		}

		template <std::derived_from<application> Application>
		int run() {
			SetConsoleOutputCP(CP_UTF8);

			Application app{};
			return app.main(static_cast<int>(args.size() - 1), args.data());
		}
	};
}  // namespace pyxel::app

#define RUN_APPLICATION(TYPE)                                                                               \
	int WINAPI wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,     \
	                    [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow) {                   \
		return pyxel::app::arg_stg{.utf8 = pyxel::app::arg_stg::wide_char_to_utf8()}.run<TYPE>();           \
	}                                                                                                       \
                                                                                                            \
	int wmain(int argc, wchar_t* argv[]) {                                                                  \
		return pyxel::app::arg_stg{.utf8 = pyxel::app::arg_stg::wide_char_to_utf8(argc, argv)}.run<TYPE>(); \
	}
#else
#define RUN_APPLICATION(TYPE)          \
	int main(int argc, char* argv[]) { \
		TYPE app{};                    \
		return app.main(argc, argv);   \
	}
#endif
