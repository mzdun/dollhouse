// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include "gltf2.hpp"
#include <assimp/BaseImporter.h>
#include <assimp/scene.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <assimp/Importer.hpp>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <numbers>
#include <optional>
#include <pyxel/base/str.hpp>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/io/export.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::io::testing {
	using gfx::light;
	using gfx::light_type;

	namespace {
		std::filesystem::path temp_file(std::string_view name) {
			auto const* info = ::testing::UnitTest::GetInstance()->current_test_info();
			auto dir = std::filesystem::temp_directory_path() / "pyxel-io-test" / info->test_suite_name();
			std::filesystem::create_directories(dir);
			auto test_name = std::string{info->name()};
			for (auto& c : test_name) {
				if (c == '/') c = '_';
			}
			return dir / std::format("{}-{}", test_name, name);
		}

		// imports with the importer of ours, unless `pyxel` is false
		aiScene const* import(Assimp::Importer& importer, std::filesystem::path const& path, bool pyxel = true) {
			if (pyxel) gltf2::register_importer(importer);
			auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
			EXPECT_NE(scene, nullptr) << importer.GetErrorString();
			return scene;
		}

		aiLight const* find_light(aiScene const& scene, std::string_view name) {
			for (unsigned index = 0; index < scene.mNumLights; ++index) {
				if (std::string_view{scene.mLights[index]->mName.C_Str()} == name) return scene.mLights[index];
			}
			return nullptr;
		}

		template <typename Value>
		std::optional<Value> meta(aiNode const& node, char const* key) {
			Value result{};
			if (!node.mMetaData || !node.mMetaData->Get(key, result)) return std::nullopt;
			return result;
		}

		aiNode const& node_of(aiScene const& scene, std::string_view name) {
			auto const* node = scene.mRootNode->FindNode(std::string{name}.c_str());
			EXPECT_NE(node, nullptr) << name;
			return *node;
		}

		void expect_color(aiColor3D const& actual, aiColor3D const& expected) {
			EXPECT_FLOAT_EQ(actual.r, expected.r);
			EXPECT_FLOAT_EQ(actual.g, expected.g);
			EXPECT_FLOAT_EQ(actual.b, expected.b);
		}

		// a panel, a disk and a spot
		gfx::sink lights_sink() {
			gfx::sink out{};
			{
				auto const scope = out.scope("panel");
				out.add_light({.type = light_type::area,
				               .color = gfx::rgb(255, 0, 0),
				               .intensity = 300.0f,
				               .range = gfx::cm(250),
				               .area = {.shape = gfx::area_shape::rectangle,
				                        .width = gfx::cm(120),
				                        .height = gfx::cm(40),
				                        .spread = gfx::deg(120)}},
				              glm::dmat4{1.0});
			}
			{
				auto const scope = out.scope("disk");
				out.add_light({.type = light_type::area,
				               .area = {.shape = gfx::area_shape::disk, .width = gfx::cm(30), .height = gfx::cm(50)}},
				              glm::dmat4{1.0});
			}
			{
				auto const scope = out.scope("lamp");
				out.add_light({.type = light_type::spot, .intensity = 60.0f}, glm::dmat4{1.0});
			}
			return out;
		}

		void write_text(std::filesystem::path const& path, std::string_view text) {
			std::ofstream file{path, std::ios::binary};
			file << text;
		}
	}  // namespace

	class gltf2_round_trip : public ::testing::TestWithParam<std::string> {};

	TEST_P(gltf2_round_trip, area_lights_come_back) {
		auto const& format = GetParam();
		auto const path = temp_file(format == "glb2" ? "lights.glb" : "lights.gltf");
		export_scene("lights", lights_sink(), gfx::materials{}, format, path);

		Assimp::Importer importer{};
		auto const* scene = import(importer, path);
		ASSERT_NE(scene, nullptr);
		ASSERT_EQ(scene->mNumLights, 3u);

		auto const* panel = find_light(*scene, "panel");
		ASSERT_NE(panel, nullptr);
		EXPECT_EQ(panel->mType, aiLightSource_AREA);
		EXPECT_FLOAT_EQ(panel->mSize.x, 1.2f);
		EXPECT_FLOAT_EQ(panel->mSize.y, 0.4f);
		expect_color(panel->mColorDiffuse, {300.0f, 0.0f, 0.0f});
		auto const& panel_node = node_of(*scene, "panel");
		EXPECT_EQ(meta<int32_t>(panel_node, gltf2::area_shape_key), gltf2::area_rectangle);
		ASSERT_TRUE(meta<float>(panel_node, gltf2::area_spread_key));
		EXPECT_NEAR(*meta<float>(panel_node, gltf2::area_spread_key), std::numbers::pi * 2 / 3, 1e-6);
		ASSERT_TRUE(meta<float>(panel_node, gltf2::range_key));
		EXPECT_FLOAT_EQ(*meta<float>(panel_node, gltf2::range_key), 2.5f);

		auto const* disk = find_light(*scene, "disk");
		ASSERT_NE(disk, nullptr);
		EXPECT_EQ(disk->mType, aiLightSource_AREA);
		// a disk is an ellipse of equal diameters
		EXPECT_FLOAT_EQ(disk->mSize.x, 0.3f);
		EXPECT_FLOAT_EQ(disk->mSize.y, 0.3f);
		expect_color(disk->mColorDiffuse, {1.0f, 1.0f, 1.0f});
		auto const& disk_node = node_of(*scene, "disk");
		EXPECT_EQ(meta<int32_t>(disk_node, gltf2::area_shape_key), gltf2::area_ellipse);
		ASSERT_TRUE(meta<float>(disk_node, gltf2::area_spread_key));
		EXPECT_FLOAT_EQ(*meta<float>(disk_node, gltf2::area_spread_key), std::numbers::pi_v<float>);
		EXPECT_FALSE(meta<float>(disk_node, gltf2::range_key));

		// read by Assimp itself
		auto const* lamp = find_light(*scene, "lamp");
		ASSERT_NE(lamp, nullptr);
		EXPECT_EQ(lamp->mType, aiLightSource_SPOT);
		expect_color(lamp->mColorDiffuse, {60.0f, 60.0f, 60.0f});
	}

	TEST_P(gltf2_round_trip, assimp_alone_has_no_area_lights) {
		auto const& format = GetParam();
		auto const path = temp_file(format == "glb2" ? "lights.glb" : "lights.gltf");
		export_scene("lights", lights_sink(), gfx::materials{}, format, path);

		Assimp::Importer importer{};
		auto const* scene = import(importer, path, false);
		ASSERT_NE(scene, nullptr);
		ASSERT_EQ(scene->mNumLights, 1u);
		EXPECT_EQ(scene->mLights[0]->mType, aiLightSource_SPOT);
	}

	INSTANTIATE_TEST_SUITE_P(formats, gltf2_round_trip, ::testing::Values("glb2", "gltf2"));

	TEST(gltf2_importer, replaces_assimp_importer_once) {
		Assimp::Importer importer{};
		auto const count = importer.GetImporterCount();
		gltf2::register_importer(importer);
		gltf2::register_importer(importer);
		EXPECT_EQ(importer.GetImporterCount(), count);

		std::vector<std::string_view> names{};
		for (size_t index = 0; index < importer.GetImporterCount(); ++index)
			names.emplace_back(importer.GetImporter(index)->GetInfo()->mName);
		EXPECT_EQ(std::ranges::count(names, gltf2::importer_name), 1);
		EXPECT_EQ(std::ranges::count(names, gltf2::base_importer_name), 0);
	}

	// Node 1 and node 2 are both `panel`; only node 2 has a light. The
	// scene has two roots, so Assimp adds a root node above them. Lights 1
	// to 3 cannot be read, and node 4 refers to a light, which is not there
	TEST(gltf2_importer, finds_nodes_by_tree_not_name) {
		auto const path = temp_file("hand-made.gltf");
		write_text(path, R"({
	"asset": {"version": "2.0"},
	"extensionsUsed": ["PYXEL_lights_area"],
	"extensions": {"PYXEL_lights_area": {"lights": [
		{"shape": "ellipse", "width": 0.5, "height": 0.25, "intensity": 2},
		{"shape": "triangle", "width": 1, "height": 1},
		{"shape": "rectangle", "height": 1},
		"not a light"
	]}},
	"scene": 0,
	"scenes": [{"nodes": [0, 1]}],
	"nodes": [
		{"name": "first", "children": [2, 3]},
		{"name": "panel", "children": [4], "extensions": {"PYXEL_lights_area": {"light": 1}}},
		{"name": "panel", "extensions": {"PYXEL_lights_area": {"light": 0}}},
		{"name": "bad", "extensions": {"PYXEL_lights_area": {"light": 2}}},
		{"name": "missing", "extensions": {"PYXEL_lights_area": {"light": 7}}}
	]
})");

		Assimp::Importer importer{};
		auto const* scene = import(importer, path);
		ASSERT_NE(scene, nullptr);
		ASSERT_EQ(scene->mNumLights, 1u);

		auto const& light = *scene->mLights[0];
		EXPECT_STREQ(light.mName.C_Str(), "panel");
		EXPECT_EQ(light.mType, aiLightSource_AREA);
		EXPECT_FLOAT_EQ(light.mSize.x, 0.5f);
		EXPECT_FLOAT_EQ(light.mSize.y, 0.25f);
		expect_color(light.mColorDiffuse, {2.0f, 2.0f, 2.0f});

		// the root added by Assimp, then `first` and the second `panel`
		ASSERT_EQ(scene->mRootNode->mNumChildren, 2u);
		auto const& first = *scene->mRootNode->mChildren[0];
		auto const& second = *scene->mRootNode->mChildren[1];
		ASSERT_EQ(first.mNumChildren, 2u);
		auto const& lit = *first.mChildren[0];
		EXPECT_STREQ(lit.mName.C_Str(), "panel");
		EXPECT_EQ(meta<int32_t>(lit, gltf2::area_shape_key), gltf2::area_ellipse);
		EXPECT_FALSE(meta<int32_t>(second, gltf2::area_shape_key));
		EXPECT_FALSE(meta<int32_t>(*first.mChildren[1], gltf2::area_shape_key));
		ASSERT_EQ(second.mNumChildren, 1u);
		EXPECT_FALSE(meta<int32_t>(*second.mChildren[0], gltf2::area_shape_key));
	}

	TEST(gltf2_importer, skips_area_light_next_to_punctual_one) {
		auto const path = temp_file("both.gltf");
		write_text(path, R"({
	"asset": {"version": "2.0"},
	"extensionsUsed": ["KHR_lights_punctual", "PYXEL_lights_area"],
	"extensions": {
		"KHR_lights_punctual": {"lights": [{"type": "point", "range": 3}]},
		"PYXEL_lights_area": {"lights": [{"shape": "rectangle", "width": 1, "height": 1, "range": 2}]}
	},
	"scenes": [{"nodes": [0]}],
	"nodes": [
		{"name": "both", "extensions": {"KHR_lights_punctual": {"light": 0}, "PYXEL_lights_area": {"light": 0}}}
	]
})");

		Assimp::Importer importer{};
		auto const* scene = import(importer, path);
		ASSERT_NE(scene, nullptr);
		ASSERT_EQ(scene->mNumLights, 1u);
		EXPECT_EQ(scene->mLights[0]->mType, aiLightSource_POINT);
		auto const& node = *scene->mRootNode;
		EXPECT_FALSE(meta<int32_t>(node, gltf2::area_shape_key));
		ASSERT_TRUE(meta<float>(node, gltf2::range_key));
		EXPECT_FLOAT_EQ(*meta<float>(node, gltf2::range_key), 3.0f);
	}

	TEST(gltf2_importer, reads_files_without_area_lights) {
		auto const path = temp_file("plain.gltf");
		write_text(path, R"({
	"asset": {"version": "2.0"},
	"scenes": [{"nodes": [0]}],
	"nodes": [{"name": "empty"}]
})");

		Assimp::Importer importer{};
		auto const* scene = import(importer, path);
		ASSERT_NE(scene, nullptr);
		EXPECT_EQ(scene->mNumLights, 0u);
		EXPECT_STREQ(scene->mRootNode->mName.C_Str(), "empty");
	}
}  // namespace pyxel::io::testing
