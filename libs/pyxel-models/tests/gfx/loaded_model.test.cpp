// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cstddef>
#include <optional>
#include <pyxel/gfx/loaded_model.hpp>
#include <string>
#include <vector>

namespace pyxel::gfx::testing {
	namespace {
		glm::mat4 translate(float x, float y, float z) {
			glm::mat4 result{1.0f};
			result[3] = glm::vec4{x, y, z, 1.0f};
			return result;
		}

		// a quarter turn around +y: x goes to -z
		glm::mat4 quarter_turn_y() {
			glm::mat4 result{1.0f};
			result[0] = glm::vec4{0.0f, 0.0f, -1.0f, 0.0f};
			result[2] = glm::vec4{1.0f, 0.0f, 0.0f, 0.0f};
			return result;
		}

		// root > frame (1 m up) > hinge (0.5 m right) > leaf; the leaf holds
		// a unit cube spanning 0..1 on every axis, and the root a lamp
		loaded_model door() {
			loaded_model::mesh cube{};
			for (auto const x : {0.0f, 1.0f}) {
				for (auto const y : {0.0f, 1.0f}) {
					for (auto const z : {0.0f, 1.0f})
						cube.positions.emplace_back(x, y, z);
				}
			}

			return {
			    .nodes = {{.name = ""},
			              {.parent = 0, .name = "frame", .local = translate(0, 1, 0)},
			              {.parent = 1, .name = "hinge", .local = translate(0.5f, 0, 0)},
			              {.parent = 2, .name = "leaf", .meshes = {0}},
			              {.parent = 0, .name = "lamp", .lights = {0}}},
			    .meshes = {cube},
			    .materials = {{.name = "Material"}},
			    .lights = {{.name = "lamp", .node = 4}},
			};
		}

		void expect_vec3(glm::vec3 const& actual, glm::vec3 const& expected) {
			EXPECT_NEAR(actual.x, expected.x, 1e-6);
			EXPECT_NEAR(actual.y, expected.y, 1e-6);
			EXPECT_NEAR(actual.z, expected.z, 1e-6);
		}
	}  // namespace

	TEST(gfx_loaded_model, find_node) {
		auto const model = door();
		EXPECT_EQ(model.find_node("hinge"), 2u);
		EXPECT_EQ(model.find_node("lamp"), 4u);
		EXPECT_EQ(model.find_node("nowhere"), std::nullopt);
	}

	TEST(gfx_loaded_model, world_matrices_follow_the_parents) {
		auto const world = door().world_matrices();
		ASSERT_EQ(world.size(), 5u);
		expect_vec3(world[3][3], {0.5f, 1.0f, 0.0f});
		expect_vec3(world[4][3], {0.0f, 0.0f, 0.0f});
	}

	TEST(gfx_loaded_model, moves_follow_the_local_matrices) {
		auto const model = door();
		std::vector<glm::mat4> moves(model.nodes.size(), glm::mat4{1.0f});
		moves[2] = quarter_turn_y();

		// the hinge turns around its own origin, so the leaf stays at the
		// hinge, and its x axis now points along -z
		auto const world = model.world_matrices(moves);
		expect_vec3(world[3][3], {0.5f, 1.0f, 0.0f});
		expect_vec3(world[3][0], {0.0f, 0.0f, -1.0f});
	}

	TEST(gfx_loaded_model, bounds) {
		auto const model = door();
		auto const still = model.bounds();
		expect_vec3(still.min, {0.5f, 1.0f, 0.0f});
		expect_vec3(still.max, {1.5f, 2.0f, 1.0f});

		std::vector<glm::mat4> moves(model.nodes.size(), glm::mat4{1.0f});
		moves[2] = quarter_turn_y();
		auto const turned = model.bounds(moves);
		expect_vec3(turned.min, {0.5f, 1.0f, -1.0f});
		expect_vec3(turned.max, {1.5f, 2.0f, 0.0f});

		EXPECT_TRUE(loaded_model{}.bounds().empty());
	}

	TEST(gfx_loaded_model, objects_find_their_nodes_and_lights) {
		auto model = door();
		interactions::geo_active_object const swing{
		    .name = "swing",
		    .states = {interactions::state("closed"), interactions::state("opened")
		                                                  .transform("hinge", interactions::channel::rotate_y(rad(1)))
		                                                  .transform("gone", interactions::channel::rotate_y(rad(1)))},
		    .actions = {{"Open", "closed", "opened"}},
		};
		std::vector<std::string> const light_names{"lamp"};
		model.resolve(interactions::load({swing}, light_names));

		ASSERT_EQ(model.objects.size(), 2u);
		auto const& geo = model.objects[0];
		EXPECT_EQ(geo.object.name, "swing");
		ASSERT_EQ(geo.nodes.size(), geo.object.groups.size());
		for (size_t index = 0; index < geo.nodes.size(); ++index) {
			auto const& group = geo.object.groups[index].group;
			EXPECT_EQ(geo.nodes[index], group == "hinge" ? std::optional<size_t>{2u} : std::nullopt) << group;
		}
		EXPECT_EQ(geo.light, std::nullopt);

		auto const& lamp = model.objects[1];
		EXPECT_EQ(lamp.object.kind, interactions::object_kind::light);
		EXPECT_EQ(lamp.nodes, std::vector<std::optional<size_t>>{});
		EXPECT_EQ(lamp.light, 0u);
	}
}  // namespace pyxel::gfx::testing
