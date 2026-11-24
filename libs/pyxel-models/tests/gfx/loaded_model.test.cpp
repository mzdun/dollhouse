// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cstddef>
#include <optional>
#include <pyxel/gfx/loaded_model.hpp>
#include <string>
#include <vector>

namespace pyxel::gfx::testing {
	using namespace math::literals;

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

	TEST(gfx_loaded_model, move_of_composes_in_the_given_order) {
		namespace ia = interactions;
		auto const point = [](glm::dmat4 const& m, glm::dvec3 const& v) {
			auto const result = m * glm::dvec4{v, 1.0};
			return glm::vec3{result};
		};

		EXPECT_EQ(move_of({}), glm::dmat4{1.0});

		// moved along x first, then turned: the origin stays 1 m along x
		std::vector<ia::group_transformation> const move_then_turn{ia::channel::translate_x(1_m),
		                                                           ia::channel::rotate_y(90_deg)};
		auto const m1 = move_of(move_then_turn);
		expect_vec3(point(m1, {0, 0, 0}), {1, 0, 0});
		expect_vec3(point(m1, {1, 0, 0}), {1, 0, -1});

		// turned first: the move goes along the turned x, that is -z
		std::vector<ia::group_transformation> const turn_then_move{ia::channel::rotate_y(90_deg),
		                                                           ia::channel::translate_x(1_m)};
		expect_vec3(point(move_of(turn_then_move), {0, 0, 0}), {0, 0, -1});

		// rotate: [x, y, z] is rotate.x, rotate.y and rotate.z, in this order
		std::vector<ia::group_transformation> const all{ia::channel::rotate(90_deg, 90_deg, 0_deg)};
		std::vector<ia::group_transformation> const each{ia::channel::rotate_x(90_deg), ia::channel::rotate_y(90_deg)};
		auto const lhs = move_of(all);
		auto const rhs = move_of(each);
		for (glm::length_t column = 0; column < 4; ++column)
			expect_vec3(glm::vec3{lhs[column]}, glm::vec3{rhs[column]});

		std::vector<ia::group_transformation> const scaled{ia::channel::scale_y(2), ia::channel::scale(3, 1, 4)};
		expect_vec3(point(move_of(scaled), {1, 1, 1}), {3, 2, 4});

		std::vector<ia::group_transformation> const moved{ia::channel::translate(offset{1_m, 2_m, 3_m})};
		expect_vec3(point(move_of(moved), {0, 0, 0}), {1, 2, 3});
	}

	TEST(gfx_loaded_model, moves_of_settled_objects) {
		namespace ia = interactions;
		auto model = door();
		ia::geo_active_object const swing{
		    .name = "swing",
		    .states = {ia::state("closed"), ia::state("opened").transform("hinge", ia::channel::rotate_y(90_deg))},
		    .actions = {{"Open", "closed", "opened"}},
		};
		ia::geo_active_object const lift{
		    .name = "lift",
		    .states = {ia::state("down"), ia::state("up").transform("hinge", ia::channel::translate_x(1_m))},
		    .actions = {{"Lift", "down", "up"}},
		};
		std::vector<std::string> const light_names{"lamp"};
		model.resolve(ia::load({swing, lift}, light_names));
		ASSERT_EQ(model.objects.size(), 3u);

		auto const starting = model.start_states();
		EXPECT_EQ(starting, (std::vector<std::optional<size_t>>{0u, 0u, std::nullopt}));
		for (auto const& move : model.moves(starting))
			EXPECT_EQ(move, glm::dmat4{1.0});

		// M = M(swing) · M(lift): the lift moves along the turned x, -z
		std::vector<std::optional<size_t>> const settled{1u, 1u, std::nullopt};
		auto const moves = model.moves(settled);
		ASSERT_EQ(moves.size(), model.nodes.size());
		auto const origin = moves[2] * glm::dvec4{0, 0, 0, 1};
		expect_vec3(glm::vec3{origin}, {0, 0, -1});
		EXPECT_EQ(moves[3], glm::dmat4{1.0});

		// unknown states and missing entries move nothing
		std::vector<std::optional<size_t>> const unknown{7u};
		for (auto const& move : model.moves(unknown))
			EXPECT_EQ(move, glm::dmat4{1.0});
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
