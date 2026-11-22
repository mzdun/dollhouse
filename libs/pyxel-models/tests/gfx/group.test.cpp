// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <numbers>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace pyxel::gfx::testing {
	using namespace math::literals;
	using points_t = std::vector<glm::dvec3>;
	using strings = std::vector<std::string>;

	namespace {
		// single vertex at the local origin, so the sink shows where the
		// matrix moved it
		struct marker : object {
			void output(sink& out, glm::dmat4 const& parent) const override {
				solid obj{};
				auto const a = obj.add_vert(offset{});
				obj.add_face(a, a, a);
				out.add_solid(obj, parent);
			}
		};

		struct thrower : object {
			void output(sink&, glm::dmat4 const&) const override { throw std::runtime_error{"child failed"}; }
		};

		glm::dmat4 const identity{1};

		glm::dvec3 apply(glm::dmat4 const& matrix, glm::dvec3 const& point) {
			return glm::dvec3{matrix * glm::dvec4{point, 1.0}};
		}

		void expect_near(glm::dvec3 const& actual, glm::dvec3 const& expected) {
			EXPECT_NEAR(actual.x, expected.x, 1e-12);
			EXPECT_NEAR(actual.y, expected.y, 1e-12);
			EXPECT_NEAR(actual.z, expected.z, 1e-12);
		}

		// the node at the path of names, starting with a child of the root
		sink::node_id find(sink const& out, strings const& names) {
			auto id = sink::root;
			for (auto const& name : names) {
				auto const& children = out.at(id).children;
				auto const it =
				    std::ranges::find_if(children, [&](sink::node_id child) { return out.at(child).name == name; });
				if (it == children.end()) throw std::out_of_range{"no node named " + name};
				id = *it;
			}
			return id;
		}

		// in the space of the node
		points_t vertices(sink const& out, std::string const& material, strings const& names) {
			return out.at(find(out, names)).meshes.at(material).vertices();
		}

		// in the space of the root
		points_t world_vertices(sink const& out, std::string const& material, strings const& names) {
			auto const id = find(out, names);
			auto const matrix = out.global_matrix(id);
			points_t result{};
			for (auto const& vertex : out.at(id).meshes.at(material).vertices())
				result.push_back(apply(matrix, vertex));
			return result;
		}

		bool has_meshes(sink const& out) {
			return std::ranges::any_of(out.nodes(), [](sink::node const& node) { return !node.meshes.empty(); });
		}
	}  // namespace

	// ---- matrix ----------------------------------------------------------

	TEST(gfx_group, default_matrix_is_identity) {
		group const grp{};
		EXPECT_EQ(grp.current_matrix(), glm::dmat4{1});
	}

	TEST(gfx_group, position_translates) {
		group const grp{1.0_m, 2.0_m, 3.0_m};
		expect_near(apply(grp.current_matrix(), {0, 0, 0}), {1, 2, 3});
		expect_near(apply(grp.current_matrix(), {1, 0, 0}), {2, 2, 3});
	}

	TEST(gfx_group, parent_matrix_applies_last) {
		group parent{};
		parent.transform(rotate_z(identity_matrix, deg(90))).transform(translate(identity_matrix, 10.0_m));
		group child{offset::at_x(1.0_m)};
		// child moves the point to x=1, the parent rotates it to y=1 and
		// then moves it by 10 along x
		expect_near(apply(child.current_matrix(parent.current_matrix()), {0, 0, 0}), {10, 1, 0});
	}

	// ---- output ----------------------------------------------------------

	TEST(gfx_group, output_passes_matrix_to_children) {
		group grp{1.0_m, 2.0_m, 3.0_m};
		grp.add<marker>();
		sink out{};
		grp.output(out, identity);
		EXPECT_EQ(vertices(out, "", {}), (points_t{{1, 2, 3}}));
	}

	TEST(gfx_group, output_uses_parent_matrix) {
		group grp{offset::at_x(1.0_m)};
		grp.add<marker>();
		glm::dmat4 parent{1};
		parent[3] = glm::dvec4{0, 5, 0, 1};
		sink out{};
		grp.output(out, parent);
		EXPECT_EQ(vertices(out, "", {}), (points_t{{1, 5, 0}}));
	}

	TEST(gfx_group, output_without_children_adds_nothing) {
		group grp{};
		grp.set_name("empty").set_material("wood");
		sink out{};
		grp.output(out, identity);
		EXPECT_FALSE(has_meshes(out));
	}

	TEST(gfx_group, output_groups_by_name_and_material) {
		group grp{};
		grp.set_name("door").set_material("wood");
		grp.add<marker>();
		sink out{};
		grp.output(out, identity);
		EXPECT_EQ(vertices(out, "wood", {"door"}), (points_t{{0, 0, 0}}));
	}

	TEST(gfx_group, unnamed_and_unpainted_groups_add_no_context) {
		group outer{};
		outer.set_name("house").set_material("brick");
		auto& inner = outer.add<group>(offset::at_x(1.0_m));
		inner.add<marker>();
		sink out{};
		outer.output(out, identity);
		EXPECT_EQ(vertices(out, "brick", {"house"}), (points_t{{1, 0, 0}}));
	}

	TEST(gfx_group, nested_groups_stack_names_and_override_material) {
		group house{offset::at_x(10.0_m)};
		house.set_name("house").set_material("brick");
		house.add<marker>();

		auto& door = house.add<group>(offset::at_x(1.0_m));
		door.set_name("door").set_material("wood");
		door.add<marker>();

		auto& handle = door.add<group>(offset::at_y(1.0_m));
		handle.set_name("handle");
		handle.add<marker>();

		house.add<marker>();  // after the door, back to the house context

		sink out{};
		house.output(out, identity);
		EXPECT_EQ(world_vertices(out, "brick", {"house"}), (points_t{{10, 0, 0}}));
		EXPECT_EQ(world_vertices(out, "wood", (strings{"house", "door"})), (points_t{{11, 0, 0}}));
		EXPECT_EQ(world_vertices(out, "wood", (strings{"house", "door", "handle"})), (points_t{{11, 1, 0}}));

		// every named group keeps its own move, the vertices stay local
		EXPECT_EQ(vertices(out, "wood", (strings{"house", "door", "handle"})), (points_t{{0, 0, 0}}));
		EXPECT_EQ(out.at(find(out, {"house"})).local, translate(identity_matrix, 10.0_m));
		EXPECT_EQ(out.at(find(out, {"house", "door"})).local, translate(identity_matrix, 1.0_m));
	}

	TEST(gfx_group, unnamed_groups_compose_into_named_node) {
		group root{};
		auto& moved = root.add<group>(offset::at_x(1.0_m));
		auto& rotated = moved.add<group>();
		rotated.transform(rotate_z(identity_matrix, deg(90)));
		auto& door = rotated.add<group>(offset::at_x(2.0_m));
		door.set_name("door");
		door.add<marker>();

		sink out{};
		root.output(out, identity);
		// moved by 2, rotated onto y, moved by 1 along x
		auto const& node = out.at(find(out, {"door"}));
		EXPECT_EQ(node.parent, sink::root);
		expect_near(apply(node.local, {0, 0, 0}), {1, 2, 0});
		EXPECT_EQ(vertices(out, "", {"door"}), (points_t{{0, 0, 0}}));
	}

	TEST(gfx_group, unnamed_group_inside_named_is_baked) {
		group door{};
		door.set_name("door");
		auto& handle = door.add<group>(offset::at_y(1.0_m));
		handle.add<marker>();

		sink out{};
		door.output(out, identity);
		EXPECT_EQ(out.nodes().size(), 2u);
		EXPECT_EQ(vertices(out, "", {"door"}), (points_t{{0, 1, 0}}));
	}

	TEST(gfx_group, same_named_siblings_are_separate_nodes) {
		group table{};
		table.add<group>(offset::at_x(1.0_m)).set_name("leg").add<marker>();
		table.add<group>(offset::at_x(-1.0_m)).set_name("leg").add<marker>();

		sink out{};
		table.output(out, identity);
		auto const& children = out.at(sink::root).children;
		ASSERT_EQ(children.size(), 2u);
		expect_near(apply(out.at(children[0]).local, {0, 0, 0}), {1, 0, 0});
		expect_near(apply(out.at(children[1]).local, {0, 0, 0}), {-1, 0, 0});
	}

	TEST(gfx_group, output_restores_context_after_exception) {
		group grp{};
		grp.set_name("broken").set_material("glass");
		grp.add<thrower>();
		sink out{};
		EXPECT_THROW(grp.output(out, identity), std::runtime_error);

		EXPECT_EQ(out.current(), sink::root);
		group after{};
		after.add<marker>();
		after.output(out, identity);
		EXPECT_EQ(vertices(out, "", {}), (points_t{{0, 0, 0}}));
	}

	// ---- children and properties ------------------------------------------

	TEST(gfx_group, add_returns_the_new_child) {
		group grp{};
		auto& child = grp.add<group>(1.0_m, 2.0_m, 3.0_m);
		ASSERT_EQ(grp.children().size(), 1u);
		EXPECT_EQ(grp.children()[0].get(), &child);
		EXPECT_EQ(child.current_matrix(), translate(identity_matrix, 1.0_m, 2.0_m, 3.0_m));
	}

	TEST(gfx_group, setters_chain) {
		group grp{};
		grp.set_name("box").set_material("wood").set_transform(scale(identity_matrix, 2));
		EXPECT_EQ(grp.name(), "box");
		EXPECT_EQ(grp.material(), "wood");
		EXPECT_EQ(grp.current_matrix(), scale(identity_matrix, 2));
	}

	// ---- transform -------------------------------------------------------

	TEST(gfx_group, transform_starts_with_the_initial_offset) {
		group const grp{offset{1.0_m, 2.0_m, 3.0_m}};
		EXPECT_EQ(grp.current_matrix(), translate(identity_matrix, 1.0_m, 2.0_m, 3.0_m));
	}

	TEST(gfx_group, transform_applies_after_the_existing_one) {
		// moved to x = 1 by the constructor, then rotated onto y
		group grp{offset::at_x(1.0_m)};
		grp.transform(rotate_z(identity_matrix, deg(90))).transform(translate(identity_matrix, offset::at_z(2.0_m)));
		expect_near(apply(grp.current_matrix(), {0, 0, 0}), {0, 1, 2});
		expect_near(apply(grp.current_matrix(), {1, 0, 0}), {0, 2, 2});
	}

	TEST(gfx_group, set_transform_replaces_the_initial_offset) {
		group grp{offset::at_x(1.0_m)};
		grp.set_transform(scale(identity_matrix, 2));
		expect_near(apply(grp.current_matrix(), {1, 1, 1}), {2, 2, 2});
	}

	TEST(gfx_group, transform_mirroring_keeps_faces_outwards) {
		group root{};
		root.transform(scale(identity_matrix, -1, 1, 1));
		root.add<solid_object>(solid::cube(1_m, 1_m, 1_m));
		sink out{};
		root.output(out, identity);
		// unnamed: the mirror is baked, and the winding flipped
		auto const& added = out.at(sink::root).meshes.at("");
		auto const& verts = added.vertices();
		for (auto const& face : added.faces()) {
			auto const normal = glm::cross(verts[face[1]] - verts[face[0]], verts[face[2]] - verts[face[0]]);
			auto center = glm::dvec3{};
			for (auto index : face)
				center += verts[index];
			center /= static_cast<double>(face.size());
			EXPECT_GT(glm::dot(normal, center - glm::dvec3{-0.5, 0.5, 0.5}), 0.0);
		}
	}

	TEST(gfx_group, named_mirror_stays_in_node) {
		group root{};
		root.set_name("mirrored").transform(scale(identity_matrix, -1, 1, 1));
		root.add<marker>();
		sink out{};
		root.output(out, identity);
		auto const& node = out.at(find(out, {"mirrored"}));
		// the consumers of the tree must observe the negative determinant
		EXPECT_LT(glm::determinant(node.local), 0.0);
		EXPECT_EQ(vertices(out, "", {"mirrored"}), (points_t{{0, 0, 0}}));
	}

	TEST(gfx_group, active_objects_keep_the_order) {
		using namespace interactions;

		group root{};
		auto const declared = geo_active_object{
		    .name = "door",
		    .states = {state("closed"), state("opened").transform("door-hinge", channel::rotate_y(deg(80)))},
		    .actions = {{"Open", "closed", "opened"}, {"Close", "opened", "closed"}},
		    .transitions = transitions{}.for_group("door-hinge", "rotate.y 200ms ease-in"),
		};

		auto& self = root.add_active_object(declared)
		                 .set_light_transition("lamp", "150ms ease-out")
		                 .add_active_object("switch", {slot{"enabled", slot_type::boolean, direction::output}});
		EXPECT_EQ(&self, &root);

		auto const& objects = root.active_objects();
		ASSERT_EQ(objects.size(), 3u);
		EXPECT_EQ(objects[0], active_object{declared});
		EXPECT_EQ(objects[1], (active_object{light_active_object{.name = "lamp", .light = "150ms ease-out"}}));

		auto const& sw = std::get<geo_active_object>(objects[2]);
		EXPECT_EQ(sw.name, "switch");
		ASSERT_EQ(sw.slots.size(), 1u);
		EXPECT_EQ(sw.slots[0].name, "enabled");
		EXPECT_TRUE(sw.states.empty());
		EXPECT_TRUE(sw.actions.empty());
		EXPECT_EQ(sw.transitions, transitions{});
	}

	TEST(gfx_group, light_transition_is_replaced) {
		using namespace interactions;

		group root{};
		root.add_active_object("lamp")
		    .set_light_transition("lamp", "150ms ease-out")
		    .set_light_transition("ceiling", "1s")
		    .set_light_transition("lamp", "300ms linear");

		auto const& objects = root.active_objects();
		ASSERT_EQ(objects.size(), 3u);
		// a geometry-based object of the same name is not a light
		EXPECT_EQ(std::get<geo_active_object>(objects[0]).name, "lamp");
		EXPECT_EQ(objects[1], (active_object{light_active_object{.name = "lamp", .light = "300ms linear"}}));
		EXPECT_EQ(objects[2], (active_object{light_active_object{.name = "ceiling", .light = "1s"}}));
	}

	TEST(gfx_group, no_active_objects_by_default) {
		EXPECT_TRUE(group{}.active_objects().empty());
		EXPECT_TRUE(group{}.add_group({.name = "child"}).active_objects().empty());
	}
}  // namespace pyxel::gfx::testing
