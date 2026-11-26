// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "sources.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <utility>
#include <vector>

namespace pyxel::io::sources::testing {
	TEST(percent_decoded, decodes_escapes) {
		EXPECT_EQ(percent_decoded("base%20color.png"), "base color.png");
		EXPECT_EQ(percent_decoded("%7e%7E"), "~~");
		EXPECT_EQ(percent_decoded("%C5%BC%C3%B3%C5%82w.png"), "\xC5\xBC\xC3\xB3\xC5\x82w.png");
	}

	TEST(percent_decoded, keeps_broken_escapes) {
		EXPECT_EQ(percent_decoded("100%"), "100%");
		EXPECT_EQ(percent_decoded("%2"), "%2");
		EXPECT_EQ(percent_decoded("%zz%2g"), "%zz%2g");
		EXPECT_EQ(percent_decoded("%%41"), "%A");
	}

	TEST(file_list, keeps_each_file_once_in_order) {
		auto const dir = std::filesystem::temp_directory_path();
		file_list files{};
		files.add(dir / "b.bin", false);
		files.add(dir / "a.gltf", true);
		files.add(dir / "sub" / ".." / "b.bin", true);
		files.add(dir / "a.gltf", false);
		files.add_u8(nullptr, true);
		files.add_u8("", true);

		auto const result = std::move(files).take();
		ASSERT_EQ(result.size(), 2u);
		EXPECT_EQ(result[0].path.filename(), "b.bin");
		EXPECT_TRUE(result[0].exists);  // found the second time
		EXPECT_EQ(result[1].path.filename(), "a.gltf");
		EXPECT_TRUE(result[1].exists);  // found once stays found
		EXPECT_TRUE(result[0].path.is_absolute());
	}
}  // namespace pyxel::io::sources::testing
