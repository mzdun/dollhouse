// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <numbers>
#include <optional>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/io/import.hpp>
#include <pyxel/io/pose.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pyxel::io::testing {
	using namespace gfx::interactions;
	using gfx::rad;

	namespace {
		std::filesystem::path temp_dir() {
			auto const* info = ::testing::UnitTest::GetInstance()->current_test_info();
			auto test_name = std::string{info->name()};
			for (auto& c : test_name) {
				if (c == '/') c = '_';
			}
			auto dir = std::filesystem::temp_directory_path() / "pyxel-io-test" / info->test_suite_name() / test_name;
			std::filesystem::remove_all(dir);
			std::filesystem::create_directories(dir);
			return dir;
		}

		std::string_view extension_of(std::string const& format) { return format == "glb2" ? "glb" : "gltf"; }

		// a frame with a leaf in the hinge, 1 m to the right, which opens by
		// a quarter turn and lifts by 10 cm
		gfx::sink door_sink() {
			gfx::group root{};
			auto& frame = root.add_group({.name = "frame"});
			frame.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(10), gfx::cm(200), gfx::cm(10)));
			auto& hinge = frame.add_group({.pos = {.x = gfx::cm(100)}, .name = "hinge"});
			hinge.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(80), gfx::cm(200), gfx::cm(4)));
			frame.add_active_object({
			    .name = "door",
			    .states = {state("closed"), state("opened")
			                                    .transform("hinge", channel::rotate_y(rad(std::numbers::pi / 2)))
			                                    .transform("hinge", channel::translate_y(gfx::cm(10)))},
			    .actions = {{"Open", "closed", "opened"}, {"Close", "opened", "closed"}},
			});

			gfx::sink out{};
			root.output(out, glm::dmat4{1.0});
			return out;
		}

		void expect_mat4(glm::mat4 const& actual, glm::mat4 const& expected) {
			for (glm::length_t column = 0; column < 4; ++column) {
				for (glm::length_t row = 0; row < 4; ++row)
					EXPECT_NEAR(actual[column][row], expected[column][row], 1e-5) << column << ", " << row;
			}
		}
	}  // namespace

	class write_posed_test : public ::testing::TestWithParam<std::string> {};

	TEST_P(write_posed_test, copy_shows_the_pose) {
		auto const dir = temp_dir();
		auto const source = dir / std::format("door.{}", extension_of(GetParam()));
		// another directory, so a `.gltf` has to find its `.bin` from there
		auto const target = dir / "posed" / std::format("door.{}", extension_of(GetParam()));
		std::filesystem::create_directories(target.parent_path());

		export_scene("door", door_sink(), gfx::materials{}, GetParam(), source);
		auto const original = import_model(source).model;
		ASSERT_EQ(original.objects.size(), 1u);
		auto const opened = original.objects[0].object.find_state("opened");
		ASSERT_TRUE(opened);

		std::vector<std::optional<size_t>> const states{opened};
		auto const moves = original.moves(states);
		write_posed(source, target, original, moves);

		auto const [copy, notes] = import_model(target);
		EXPECT_TRUE(notes.empty());
		// the extra is left out, so the states do not move the pose again
		EXPECT_TRUE(copy.objects.empty());
		ASSERT_EQ(copy.nodes.size(), original.nodes.size());
		EXPECT_EQ(copy.meshes.size(), original.meshes.size());

		std::vector<glm::mat4> float_moves(moves.begin(), moves.end());
		auto const expected = original.world_matrices(float_moves);
		auto const actual = copy.world_matrices();
		for (size_t index = 0; index < copy.nodes.size(); ++index) {
			SCOPED_TRACE(copy.nodes[index].name);
			EXPECT_EQ(copy.nodes[index].name, original.nodes[index].name);
			expect_mat4(actual[index], expected[index]);
		}

		// the leaf is turned: 80 cm along -z, and lifted by 10 cm
		auto const box = copy.bounds();
		EXPECT_NEAR(box.min.z, -0.8f, 1e-5);
		EXPECT_NEAR(box.max.y, 2.1f, 1e-5);
	}

	TEST_P(write_posed_test, start_states_keep_the_nodes) {
		auto const dir = temp_dir();
		auto const source = dir / std::format("door.{}", extension_of(GetParam()));
		auto const target = dir / std::format("copy.{}", extension_of(GetParam()));

		export_scene("door", door_sink(), gfx::materials{}, GetParam(), source);
		auto const original = import_model(source).model;
		write_posed(source, target, original, original.moves(original.start_states()));

		auto const copy = import_model(target).model;
		EXPECT_TRUE(copy.objects.empty());
		ASSERT_EQ(copy.nodes.size(), original.nodes.size());
		for (size_t index = 0; index < copy.nodes.size(); ++index)
			expect_mat4(copy.nodes[index].local, original.nodes[index].local);
	}

	TEST_P(write_posed_test, copy_keeps_the_container) {
		auto const dir = temp_dir();
		auto const source = dir / std::format("door.{}", extension_of(GetParam()));
		auto const other = dir / std::format("copy.{}", GetParam() == "glb2" ? "gltf" : "glb");

		export_scene("door", door_sink(), gfx::materials{}, GetParam(), source);
		auto const original = import_model(source).model;
		EXPECT_THROW(write_posed(source, other, original, original.moves(original.start_states())), std::runtime_error);
		EXPECT_FALSE(std::filesystem::exists(other));
	}

	INSTANTIATE_TEST_SUITE_P(formats, write_posed_test, ::testing::Values("glb2", "gltf2"));

	TEST(write_posed, other_files_throw) {
		auto const dir = temp_dir();
		auto const source = dir / "notes.gltf";
		std::ofstream{source} << "not JSON at all";

		gfx::loaded_model const empty{};
		EXPECT_THROW(write_posed(source, dir / "copy.gltf", empty, {}), std::runtime_error);
		EXPECT_THROW(write_posed(dir / "missing.gltf", dir / "copy.gltf", empty, {}), std::runtime_error);
	}

	TEST(write_posed, missing_node_throws) {
		auto const dir = temp_dir();
		auto const source = dir / "door.glb";
		export_scene("door", door_sink(), gfx::materials{}, "glb2", source);

		auto model = import_model(source).model;
		model.nodes.push_back({.parent = 0, .name = "nowhere"});
		std::vector<glm::dmat4> moves(model.nodes.size(), glm::dmat4{1.0});
		moves.back() = glm::dmat4{2.0};
		EXPECT_THROW(write_posed(source, dir / "copy.glb", model, moves), std::runtime_error);
	}
}  // namespace pyxel::io::testing
