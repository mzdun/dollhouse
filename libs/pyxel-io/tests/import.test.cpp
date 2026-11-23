// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cstddef>
#include <filesystem>
#include <format>
#include <numbers>
#include <optional>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/io/import.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::io::testing {
	using namespace gfx::interactions;
	using gfx::rad;

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

		std::filesystem::path file_for(std::string const& format, std::string_view stem) {
			return temp_file(std::format("{}.{}", stem, format == "glb2" ? "glb" : "gltf"));
		}

		geo_active_object door() {
			return {
			    .name = "door",
			    .states = {state("closed"), state("opened").transform("hinge", channel::rotate_y(rad(1.5)))},
			    .actions = {{"Open", "closed", "opened"}, {"Close", "opened", "closed"}},
			};
		}

		// a frame, 10 x 200 x 10 cm, with a leaf in the hinge, 1 m to the
		// right, and a lamp in a group of its own
		gfx::sink door_sink() {
			gfx::group root{};
			auto& frame = root.add_group({.name = "frame"});
			frame.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(10), gfx::cm(200), gfx::cm(10)));
			auto& hinge = frame.add_group({.pos = {.x = gfx::cm(100)}, .name = "hinge"});
			hinge.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(80), gfx::cm(200), gfx::cm(4)));
			frame.add_group({.name = "lamp"}).add(gfx::light{.name = "lamp"});
			frame.add_active_object(door()).set_light_transition("lamp", "150ms");

			gfx::sink out{};
			root.output(out, glm::dmat4{1.0});
			return out;
		}

		void expect_vec3(glm::vec3 const& actual, glm::vec3 const& expected) {
			EXPECT_NEAR(actual.x, expected.x, 1e-5);
			EXPECT_NEAR(actual.y, expected.y, 1e-5);
			EXPECT_NEAR(actual.z, expected.z, 1e-5);
		}
	}  // namespace

	class import_model_test : public ::testing::TestWithParam<std::string> {};

	TEST_P(import_model_test, nodes_come_in_depth_first_order) {
		auto const path = file_for(GetParam(), "door");
		export_scene("door", door_sink(), gfx::materials{}, GetParam(), path);
		auto const [model, notes] = import_model(path);

		ASSERT_FALSE(model.nodes.empty());
		EXPECT_EQ(model.nodes[0].parent, std::nullopt);
		for (size_t index = 1; index < model.nodes.size(); ++index) {
			ASSERT_TRUE(model.nodes[index].parent) << index;
			EXPECT_LT(*model.nodes[index].parent, index);
		}

		auto const frame = model.find_node("frame");
		auto const hinge = model.find_node("hinge");
		auto const lamp = model.find_node("lamp");
		ASSERT_TRUE(frame && hinge && lamp);
		EXPECT_EQ(model.nodes[*hinge].parent, frame);
		EXPECT_EQ(model.nodes[*lamp].parent, frame);
		expect_vec3(model.world_matrices()[*hinge][3], {1.0f, 0.0f, 0.0f});
		EXPECT_EQ(notes, std::vector<interactions::note>{});
	}

	TEST_P(import_model_test, meshes_are_triangles_with_normals) {
		auto const path = file_for(GetParam(), "door");
		export_scene("door", door_sink(), gfx::materials{}, GetParam(), path);
		auto const model = import_model(path).model;

		auto const frame = model.find_node("frame");
		ASSERT_TRUE(frame);
		ASSERT_EQ(model.nodes[*frame].meshes.size(), 1u);
		auto const& mesh = model.meshes[model.nodes[*frame].meshes[0]];
		// six faces of two triangles each
		EXPECT_EQ(mesh.indices.size(), 36u);
		EXPECT_EQ(mesh.normals.size(), mesh.positions.size());
		ASSERT_LT(mesh.material, model.materials.size());

		auto const box = model.bounds();
		ASSERT_FALSE(box.empty());
		expect_vec3(box.size(), {1.8f, 2.0f, 0.1f});
	}

	TEST_P(import_model_test, objects_find_their_nodes_and_lights) {
		auto const path = file_for(GetParam(), "door");
		export_scene("door", door_sink(), gfx::materials{}, GetParam(), path);
		auto const model = import_model(path).model;

		ASSERT_EQ(model.lights.size(), 1u);
		EXPECT_EQ(model.lights[0].name, "lamp");
		EXPECT_EQ(model.lights[0].node, model.find_node("lamp"));
		EXPECT_EQ(model.nodes[model.lights[0].node].lights, std::vector<size_t>{0u});

		ASSERT_EQ(model.objects.size(), 2u);
		auto const& swing = model.objects[0];
		EXPECT_EQ(swing.object.name, "door");
		ASSERT_EQ(swing.object.groups.size(), 1u);
		EXPECT_EQ(swing.nodes, (std::vector<std::optional<size_t>>{model.find_node("hinge")}));

		auto const& lamp = model.objects[1];
		EXPECT_EQ(lamp.object.kind, object_kind::light);
		EXPECT_EQ(lamp.light, 0u);
		EXPECT_DOUBLE_EQ(lamp.object.light.duration, 0.15);
	}

	TEST_P(import_model_test, area_lights_come_back) {
		gfx::sink out{};
		{
			auto const scope = out.scope("panel");
			out.add_light({.type = gfx::light_type::area,
			               .range = gfx::cm(250),
			               .area = {.shape = gfx::area_shape::rectangle,
			                        .width = gfx::cm(120),
			                        .height = gfx::cm(40),
			                        .spread = gfx::deg(120)}},
			              glm::dmat4{1.0});
		}
		auto const path = file_for(GetParam(), "panel");
		export_scene("panel", out, gfx::materials{}, GetParam(), path);
		auto const model = import_model(path).model;

		ASSERT_EQ(model.lights.size(), 1u);
		auto const& panel = model.lights[0];
		EXPECT_EQ(panel.name, "panel");
		EXPECT_EQ(panel.type, gfx::light_type::area);
		EXPECT_EQ(panel.shape, gfx::area_shape::rectangle);
		EXPECT_FLOAT_EQ(panel.width, 1.2f);
		EXPECT_FLOAT_EQ(panel.height, 0.4f);
		EXPECT_NEAR(panel.spread, std::numbers::pi * 2 / 3, 1e-6);
		ASSERT_TRUE(panel.range);
		EXPECT_FLOAT_EQ(*panel.range, 2.5f);
	}

	INSTANTIATE_TEST_SUITE_P(formats, import_model_test, ::testing::Values("glb2", "gltf2"));

	TEST(import_model, missing_file_throws) {
		EXPECT_THROW(import_model(temp_file("missing.glb")), std::runtime_error);
	}
}  // namespace pyxel::io::testing
