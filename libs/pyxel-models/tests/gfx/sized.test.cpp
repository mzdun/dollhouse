// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/sized.hpp>
#include <type_traits>

namespace pyxel::gfx::testing {
	using namespace math::literals;

	namespace {
		struct box : sized {
			using sized::sized;

			// lets the tests see that chaining keeps the subclass type
			box& tag() noexcept {
				tagged = true;
				return *this;
			}

			void output(sink&, glm::dmat4 const&) const override {}

			bool tagged{false};
		};
	}  // namespace

	TEST(gfx_sized, from_size) {
		box const obj{size{1_cm, 2_cm, 3_cm}};
		EXPECT_EQ(obj.size(), (size{1_cm, 2_cm, 3_cm}));
	}

	TEST(gfx_sized, from_edges) {
		box const obj{1_cm, 2_cm, 3_cm};
		EXPECT_EQ(obj.size(), (size{1_cm, 2_cm, 3_cm}));
	}

	TEST(gfx_sized, set_size) {
		box obj{1_cm, 2_cm, 3_cm};
		obj.set_size(size{4_cm, 5_cm, 6_cm});
		EXPECT_EQ(obj.size(), (size{4_cm, 5_cm, 6_cm}));
		obj.set_size(7_cm, 8_cm, 9_cm);
		EXPECT_EQ(obj.size(), (size{7_cm, 8_cm, 9_cm}));
	}

	TEST(gfx_sized, resize_keeps_empty_edges) {
		box obj{1_cm, 2_cm, 3_cm};
		obj.resize({.height = 10_cm});
		EXPECT_EQ(obj.size(), (size{1_cm, 10_cm, 3_cm}));
		obj.resize({.width = 4_cm, .depth = 6_cm});
		EXPECT_EQ(obj.size(), (size{4_cm, 10_cm, 6_cm}));
		obj.resize({});
		EXPECT_EQ(obj.size(), (size{4_cm, 10_cm, 6_cm}));
	}

	TEST(gfx_sized, grow_by) {
		box obj{1_cm, 2_cm, 3_cm};
		obj.grow_by(size{1_cm, 1_cm, 1_cm});
		EXPECT_EQ(obj.size(), (size{2_cm, 3_cm, 4_cm}));
		obj.grow_by(-1_cm, 0_cm, 2_cm);
		EXPECT_EQ(obj.size(), (size{1_cm, 3_cm, 6_cm}));
	}

	TEST(gfx_sized, scale_uniform) {
		box obj{1_cm, 2_cm, 3_cm};
		obj.scale(2.0);
		EXPECT_EQ(obj.size(), (size{2_cm, 4_cm, 6_cm}));
	}

	TEST(gfx_sized, scale_per_axis) {
		box obj{1_cm, 2_cm, 3_cm};
		obj.scale(size::factors{.x = 3.0, .z = 0.5});
		EXPECT_EQ(obj.size(), (size{3_cm, 2_cm, 1.5_cm}));
	}

	TEST(gfx_sized, chaining_keeps_subclass) {
		box obj{1_cm, 1_cm, 1_cm};
		static_assert(std::is_same_v<decltype(obj.set_size(size{})), box&>);
		static_assert(std::is_same_v<decltype(obj.scale(2.0)), box&>);

		auto& result = obj.set_size(1_cm, 2_cm, 3_cm).grow_by(1_cm, 1_cm, 1_cm).scale(2.0).tag();
		EXPECT_EQ(&result, &obj);
		EXPECT_TRUE(obj.tagged);
		EXPECT_EQ(obj.size(), (size{4_cm, 6_cm, 8_cm}));
	}
}  // namespace pyxel::gfx::testing
