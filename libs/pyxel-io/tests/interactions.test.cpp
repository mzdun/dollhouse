// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <assimp/Importer.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <pyxel/base/str.hpp>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/io/interactions.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "gltf2.hpp"

namespace pyxel::io::testing {
	using namespace gfx::interactions;
	using gfx::deg;
	using gfx::length;
	using gfx::offset;
	using gfx::rad;
	using io::interactions::serialize;

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

		// the JSON document of a `.gltf` file, or the JSON chunk of a `.glb`
		rapidjson::Document read_document(std::filesystem::path const& path) {
			std::ifstream file{path, std::ios::binary};
			std::string bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
			if (path.extension() == ".glb") {
				std::uint32_t length{};
				if (bytes.size() < 20) throw std::runtime_error{"the binary is cut short"};
				std::memcpy(&length, bytes.data() + 12, sizeof(length));
				bytes = bytes.substr(20, length);
			}

			rapidjson::Document doc{};
			doc.Parse(bytes.data(), bytes.size());
			if (doc.HasParseError()) throw std::runtime_error{"the JSON document cannot be parsed"};
			return doc;
		}

		rapidjson::Value const& root_node(rapidjson::Document const& doc) {
			auto const& scene = doc["scenes"][doc.HasMember("scene") ? doc["scene"].GetUint() : 0u];
			return doc["nodes"][scene["nodes"][0].GetUint()];
		}

		rapidjson::Value const* extra(rapidjson::Value const& node) {
			if (!node.HasMember("extras")) return nullptr;
			auto const& extras = node["extras"];
			auto const it = extras.FindMember(gltf2::interactions_extra);
			return it == extras.MemberEnd() ? nullptr : &it->value;
		}

		bool has_node(rapidjson::Document const& doc, std::string_view name) {
			for (auto const& node : doc["nodes"].GetArray()) {
				if (node.HasMember("name") && std::string_view{node["name"].GetString()} == name) return true;
			}
			return false;
		}

		geo_active_object door() {
			return {
			    .name = "door",
			    .states = {state("closed"), state("opened").transform("hinge", channel::rotate_y(rad(1.5)))},
			    .actions = {{"Open", "closed", "opened"}, {"Close", "opened", "closed"}},
			    .transitions = transitions{}.for_group("hinge", "rotate.y 200ms ease-in"),
			};
		}

		// a door leaf in a named, empty hinge, and a frame, which no object
		// names
		gfx::sink door_sink(bool with_interactions) {
			gfx::group root{};
			auto& frame = root.add_group({.name = "frame"});
			frame.add<gfx::solid_object>(gfx::solid::cube(gfx::cm(10), gfx::cm(200), gfx::cm(10)));
			auto& hinge = frame.add_group({.name = "hinge"});
			hinge.add_group({.name = "unused"});
			if (with_interactions) frame.add_active_object(door()).set_light_transition("lamp", "150ms");

			gfx::sink out{};
			root.output(out, glm::dmat4{1.0});
			return out;
		}
	}  // namespace

	TEST(interactions_serialize, empty_list) { EXPECT_EQ(serialize({}), "[]"); }

	TEST(interactions_serialize, light) {
		EXPECT_EQ(
		    serialize({light_active_object{.name = "bulb", .light = "150ms"}, light_active_object{.name = "lamp"}}),
		    R"([{"name":"bulb","version":"1.0","transitions":{"light":"150ms"}},)"
		    R"({"name":"lamp","version":"1.0"}])");
	}

	TEST(interactions_serialize, empty_geo_object) {
		EXPECT_EQ(serialize({geo_active_object{.name = "nothing"}}), R"([{"name":"nothing","version":"1.0"}])");
	}

	TEST(interactions_serialize, switch_object) {
		auto const declaration = geo_active_object{
		    .name = "switch",
		    .slots = {slot{"enabled", slot_type::boolean, direction::output}},
		    .states = {state("switched-off"),
		               state("switched-on").transform("rocker", channel::rotate_x(rad(0.25))).set("enabled", true)},
		    .actions = {{"Turn on", "switched-off", "switched-on"}, {"Turn off", "switched-on", "switched-off"}},
		    .transitions = transitions{}.for_group("rocker", "rotate.x 50ms").for_slot("enabled", "0ms linear 45ms"),
		};

		EXPECT_EQ(
		    serialize({declaration}),
		    R"([{"name":"switch","version":"1.0",)"
		    R"("slots":[{"name":"enabled","type":"boolean","direction":"output"}],)"
		    R"("states":{"switched-off":{},"switched-on":{"rocker":[{"rotate.x":0.25}],"$enabled":[{"set":true}]}},)"
		    R"("actions":[{"label":"Turn on","when":"switched-off","then":"switched-on"},)"
		    R"({"label":"Turn off","when":"switched-on","then":"switched-off"}],)"
		    R"("transitions":{"rocker":"rotate.x 50ms","$enabled":"0ms linear 45ms"}}])");
	}

	TEST(interactions_serialize, states_keep_their_order) {
		auto const declaration = geo_active_object{.name = "dial", .states = {state("z"), state("a"), state("1")}};
		EXPECT_EQ(serialize({declaration}), R"([{"name":"dial","version":"1.0","states":{"z":{},"a":{},"1":{}}}])");
	}

	TEST(interactions_serialize, every_channel) {
		auto const declaration = geo_active_object{
		    .name = "all",
		    .states = {state("moved").transform("part",
		                                        std::vector<group_transformation>{
		                                            channel::translate_x(length{0.5}),
		                                            channel::translate_y(length{-1}),
		                                            channel::translate_z(length{2}),
		                                            channel::translate(length{0.25}),
		                                            channel::rotate_x(rad(0.5)),
		                                            channel::rotate_y(rad(-0.5)),
		                                            channel::rotate_z(rad(1)),
		                                            channel::rotate(rad(0.5), rad(1)),
		                                            channel::scale_x(2),
		                                            channel::scale_y(0),
		                                            channel::scale_z(-1),
		                                            channel::scale(2, 3),
		                                        })},
		};

		EXPECT_EQ(serialize({declaration}),
		          R"([{"name":"all","version":"1.0","states":{"moved":{"part":[)"
		          R"({"translate.x":0.5},{"translate.y":-1.0},{"translate.z":2.0},{"translate":[0.25,0.0,0.0]},)"
		          R"({"rotate.x":0.5},{"rotate.y":-0.5},{"rotate.z":1.0},{"rotate":[0.5,1.0,0.0]},)"
		          R"({"scale.x":2.0},{"scale.y":0.0},{"scale.z":-1.0},{"scale":[2.0,3.0,1.0]}]}}}])");
	}

	TEST(interactions_serialize, slot_values_and_defaults) {
		auto const declaration = geo_active_object{
		    .name = "values",
		    .slots =
		        {
		            slot{"level", slot_type::factor, direction::input, 0.5},
		            slot{"speed", slot_type::floating, direction::variable},
		            slot{"tilt", slot_type::angle, direction::output, rad(0.25)},
		            slot{"depth", slot_type::length, direction::output, length{2}},
		            slot{"at", slot_type::offset, direction::output, offset{length{1}, length{2}, length{3}}},
		        },
		};

		EXPECT_EQ(serialize({declaration}),
		          R"([{"name":"values","version":"1.0","slots":[)"
		          R"({"name":"level","type":"factor","direction":"input","default":0.5},)"
		          R"({"name":"speed","type":"float","direction":"variable"},)"
		          R"({"name":"tilt","type":"angle","direction":"output","default":0.25},)"
		          R"({"name":"depth","type":"length","direction":"output","default":2.0},)"
		          R"({"name":"at","type":"offset","direction":"output","default":[1.0,2.0,3.0]}]}])");
	}

	TEST(interactions_serialize, unordered_keys_are_sorted) {
		auto const declaration = geo_active_object{
		    .name = "sorted",
		    .states = {state("on")
		                   .set("b", true)
		                   .set("a", false)
		                   .transform("y", channel::scale_x(2))
		                   .transform("x", channel::scale_x(3))},
		    .transitions =
		        transitions{}.for_slot("b", "1s").for_slot("a", "2s").for_group("y", "3s").for_group("x", "4s"),
		};

		EXPECT_EQ(serialize({declaration}),
		          R"([{"name":"sorted","version":"1.0","states":{"on":{)"
		          R"("x":[{"scale.x":3.0}],"y":[{"scale.x":2.0}],"$a":[{"set":false}],"$b":[{"set":true}]}},)"
		          R"("transitions":{"x":"4s","y":"3s","$a":"2s","$b":"1s"}}])");
	}

	TEST(interactions_serialize, slot_references) {
		// a slot named with a "$" gets one more; "*" is the defaults
		auto const declaration = geo_active_object{
		    .name = "refs",
		    .states = {state("on").set("$odd", 1.0)},
		    .transitions = transitions{}.for_all("100ms").for_slot("$odd", "1s"),
		};

		EXPECT_EQ(serialize({declaration}),
		          R"([{"name":"refs","version":"1.0","states":{"on":{"$$odd":[{"set":1.0}]}},)"
		          R"("transitions":{"*":"100ms","$$odd":"1s"}}])");
	}

	TEST(interactions_serialize, when_with_several_states) {
		auto const declaration = geo_active_object{
		    .name = "window",
		    .actions = {{"Close", {"tilted", "opened"}, "closed"}},
		};

		EXPECT_EQ(serialize({declaration}), R"([{"name":"window","version":"1.0","actions":[)"
		                                    R"({"label":"Close","when":["tilted","opened"],"then":"closed"}]}])");
	}

	class interactions_export : public ::testing::TestWithParam<std::string> {};

	TEST_P(interactions_export, extra_goes_to_root_node) {
		auto const& format = GetParam();
		auto const path = temp_file(format == "glb2" ? "door.glb" : "door.gltf");
		export_scene("door", door_sink(true), gfx::materials{}, format, path);

		auto const doc = read_document(path);
		auto const* stored = extra(root_node(doc));
		ASSERT_NE(stored, nullptr);

		rapidjson::Document expected{};
		auto const text = serialize({door(), light_active_object{.name = "lamp", .light = "150ms"}});
		expected.Parse(text.data(), text.size());
		EXPECT_TRUE(*stored == expected);

		// the hinge is empty, but the door moves it; nothing names "unused"
		EXPECT_TRUE(has_node(doc, "hinge"));
		EXPECT_FALSE(has_node(doc, "unused"));

		// the extra does not get in the way of reading the file back
		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		EXPECT_NE(importer.ReadFile(as_str(path.u8string()), 0u), nullptr) << importer.GetErrorString();
	}

	TEST_P(interactions_export, no_extra_without_active_objects) {
		auto const& format = GetParam();
		auto const path = temp_file(format == "glb2" ? "door.glb" : "door.gltf");
		export_scene("door", door_sink(false), gfx::materials{}, format, path);

		auto const doc = read_document(path);
		EXPECT_EQ(extra(root_node(doc)), nullptr);
		EXPECT_FALSE(has_node(doc, "hinge"));
	}

	INSTANTIATE_TEST_SUITE_P(formats, interactions_export, ::testing::Values("glb2", "gltf2"));

	TEST(interactions_export, other_formats_refuse_active_objects) {
		auto const path = temp_file("door.obj");
		EXPECT_THROW(export_scene("door", door_sink(true), gfx::materials{}, "obj", path), std::runtime_error);
		EXPECT_NO_THROW(export_scene("door", door_sink(false), gfx::materials{}, "obj", path));
	}
}  // namespace pyxel::io::testing
