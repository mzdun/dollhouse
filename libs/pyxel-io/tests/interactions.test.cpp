// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <assimp/scene.h>
#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <assimp/Importer.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <pyxel/base/str.hpp>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/light.hpp>
#include <pyxel/gfx/sink.hpp>
#include <pyxel/gfx/solid_object.hpp>
#include <pyxel/io/export.hpp>
#include <pyxel/io/interactions.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "gltf2.hpp"

namespace pyxel::io::testing {
	using namespace gfx::interactions;
	using gfx::deg;
	using gfx::length;
	using gfx::offset;
	using gfx::rad;
	using io::interactions::note;
	using io::interactions::parse;
	using io::interactions::parsed;
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

		void write_text(std::filesystem::path const& path, std::string_view text) {
			std::ofstream file{path, std::ios::binary};
			file << text;
		}

		std::vector<std::string> const lights{"lamp"};

		// the declarations, after a round trip through the JSON
		parsed round_trip(active_objects const& objects) { return parse(serialize(objects), lights); }

		note dropped(std::string object, std::string reason) {
			return {.object = std::move(object), .what = note::dropped, .reason = std::move(reason)};
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
			frame.add_group({.name = "lamp"}).add(gfx::light{.name = "lamp"});
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

	TEST(interactions_parse, round_trip) {
		active_objects const objects{
		    geo_active_object{
		        .name = "switch",
		        .slots =
		            {
		                slot{"enabled", slot_type::boolean, direction::output},
		                slot{"level", slot_type::factor, direction::input, 0.5},
		                slot{"tilt", slot_type::angle, direction::variable, rad(0.25)},
		                slot{"depth", slot_type::length, direction::output, length{2}},
		                slot{"at", slot_type::offset, direction::output, offset{length{1}, length{2}, length{3}}},
		            },
		        .states = {state("switched-off"), state("switched-on")
		                                              .transform("rocker", channel::rotate_x(rad(0.25)))
		                                              .transform("rocker", channel::translate(length{0.5}))
		                                              .transform("base", channel::scale(2, 3))
		                                              .set("enabled", true)
		                                              .set("tilt", rad(0.5))
		                                              .set("undeclared", 2.0)},
		        .actions = {{"Turn on", "switched-off", "switched-on"},
		                    {"Turn off", {"switched-on", "switched-off"}, "switched-off"}},
		        .transitions = transitions{}
		                           .for_all("100ms")
		                           .for_group("rocker", "rotate.x 50ms")
		                           .for_slot("enabled", "0ms linear 45ms")
		                           .for_slot("$odd", "1s"),
		    },
		    geo_active_object{.name = "empty"},
		    light_active_object{.name = "lamp", .light = "150ms"},
		};

		EXPECT_EQ(round_trip(objects), (parsed{.objects = objects}));
		EXPECT_EQ(round_trip({light_active_object{.name = "lamp"}}),
		          (parsed{.objects = {light_active_object{.name = "lamp"}}}));
	}

	TEST(interactions_parse, lights_are_found_by_name) {
		auto const text = R"([{"name": "lamp", "version": "1.0"}, {"name": "bulb", "version": "1.0"}])";
		EXPECT_EQ(parse(text, lights).objects,
		          (active_objects{light_active_object{.name = "lamp"}, geo_active_object{.name = "bulb"}}));
		EXPECT_EQ(parse(text, {}).objects,
		          (active_objects{geo_active_object{.name = "lamp"}, geo_active_object{.name = "bulb"}}));
	}

	TEST(interactions_parse, vectors_are_filled) {
		auto const result = parse(
		    R"([{"name": "v", "version": "1.0", "states": {"s": {"g": [{"translate": [1]}, {"rotate": [0.5, 1]}, {"scale": [2]}]}}}])",
		    lights);
		ASSERT_EQ(result.objects.size(), 1u);
		auto const& changes = std::get<geo_active_object>(result.objects[0]).states.at(0).groups.at("g");
		EXPECT_EQ(changes, (std::vector<group_transformation>{
		                       channel::translate(length{1}),
		                       channel::rotate(rad(0.5), rad(1)),
		                       channel::scale(2),
		                   }));
	}

	TEST(interactions_parse, text_which_is_not_a_list_throws) {
		EXPECT_THROW(parse("[{", lights), std::runtime_error);
		EXPECT_THROW(parse(R"({"name": "door"})", lights), std::runtime_error);
		EXPECT_EQ(parse("[]", lights), parsed{});
	}

	TEST(interactions_parse, unreadable_versions_are_dropped) {
		auto const result = parse(R"([
			{"name": "no-version"},
			{"name": "number", "version": 1.0},
			{"name": "short", "version": "1"},
			{"name": "leading-zero", "version": "01.0"},
			{"name": "patch", "version": "1.0.0"},
			{"name": "major", "version": "2.0"},
			{"name": "old", "version": "0.9"},
			{"version": "1.0"},
			"not an object",
			{"name": "good", "version": "1.0"}
		])",
		                          lights);

		EXPECT_EQ(result.objects, (active_objects{geo_active_object{.name = "good"}}));
		EXPECT_EQ(result.notes, (std::vector<note>{
		                            dropped("no-version", "it has no version"),
		                            dropped("number", "it has no version"),
		                            dropped("short", "\"1\" is not a version"),
		                            dropped("leading-zero", "\"01.0\" is not a version"),
		                            dropped("patch", "\"1.0.0\" is not a version"),
		                            dropped("major", "version 2.0 is not supported"),
		                            dropped("old", "version 0.9 is too old, and cannot be upgraded"),
		                            dropped("", "it has no name"),
		                            dropped("", "the entry is not a mapping"),
		                        }));
	}

	TEST(interactions_parse, own_version_fails_on_unknown_names) {
		auto const result = parse(R"([
			{"name": "key", "version": "1.0", "updates": {}},
			{"name": "type", "version": "1.0", "slots": [{"name": "n", "type": "int", "direction": "input"}]},
			{"name": "channel", "version": "1.0", "states": {"s": {"g": [{"visible": false}]}}},
			{"name": "action", "version": "1.0", "actions": [{"label": "Go", "when": "a", "then": "b", "key": 1}]}
		])",
		                          lights);

		EXPECT_TRUE(result.objects.empty());
		EXPECT_EQ(result.notes, (std::vector<note>{
		                            dropped("key", "has an unknown key `updates`"),
		                            dropped("type", "has an unknown type `int` of slot `n`"),
		                            dropped("channel", "has an unknown channel `visible` of `g` in state `s`"),
		                            dropped("action", "has an unknown key `key` of action `Go`"),
		                        }));
	}

	TEST(interactions_parse, newer_minor_version_ignores_unknown_names) {
		auto const result = parse(R"([{
			"name": "newer", "version": "1.3", "updates": {},
			"slots": [
				{"name": "count", "type": "int", "direction": "output"},
				{"name": "on", "type": "boolean", "direction": "output", "unit": "none"}
			],
			"states": {"s": {
				"g": [{"visible": false}, {"rotate.y": 1}],
				"$count": [{"set": 2}],
				"$on": [{"set": true}]
			}},
			"transitions": {"$count": "1s", "$on": "2s"}
		}])",
		                          lights);

		auto const expected = geo_active_object{
		    .name = "newer",
		    .slots = {slot{"on", slot_type::boolean, direction::output}},
		    .states = {state("s").transform("g", channel::rotate_y(rad(1))).set("on", true)},
		    .transitions = transitions{}.for_slot("on", "2s"),
		};
		EXPECT_EQ(result.objects, (active_objects{expected}));
		EXPECT_EQ(
		    result.notes,
		    (std::vector<note>{{.object = "newer",
		                        .what = note::ignored,
		                        .reason = "version 1.3 is newer; left out: type `int` of slot `count`, key `unit` "
		                                  "of slot `on`, key `updates`, channel `visible` of `g` in state `s`"}}));
	}

	// a group or a slot, whose every change is left out, is not in the state
	// at all: re-exported, it would be written as an empty list
	TEST(interactions_parse, nothing_left_of_a_change_leaves_no_target) {
		auto const result = parse(R"([{
			"name": "newer", "version": "1.1",
			"slots": [{"name": "level", "type": "factor", "direction": "output"}],
			"states": {"on": {
				"g": [{"visible": false}],
				"h": [{"rotate.y": 1}],
				"$level": [{"ramp": 1}]
			}}
		}])",
		                          lights);

		auto const expected = geo_active_object{
		    .name = "newer",
		    .slots = {slot{"level", slot_type::factor, direction::output}},
		    .states = {state("on").transform("h", channel::rotate_y(rad(1)))},
		};
		EXPECT_EQ(result.objects, (active_objects{expected}));
		ASSERT_EQ(result.notes.size(), 1u);
		EXPECT_EQ(result.notes[0].what, note::ignored);
	}

	TEST(interactions_parse, shapes_which_do_not_fit_are_dropped) {
		auto const result = parse(R"([
			{"name": "lamp", "version": "1.0", "states": {}},
			{"name": "lamp", "version": "1.0", "transitions": {"*": "1s"}},
			{"name": "label", "version": "1.0", "actions": [{"when": "a", "then": "b"}]},
			{"name": "default", "version": "1.0", "slots": [{"name": "n", "type": "angle", "direction": "input", "default": true}]},
			{"name": "set", "version": "1.0", "slots": [{"name": "n", "type": "offset", "direction": "output"}],
			 "states": {"s": {"$n": [{"set": 1}]}}},
			{"name": "vector", "version": "1.0", "states": {"s": {"g": [{"scale": [1, 2, 3, 4]}]}}},
			{"name": "two", "version": "1.0", "states": {"s": {"g": [{"scale.x": 1, "scale.y": 1}]}}}
		])",
		                          lights);

		EXPECT_TRUE(result.objects.empty());
		EXPECT_EQ(result.notes, (std::vector<note>{
		                            dropped("lamp", "describes a light, but has `states`"),
		                            dropped("lamp", "describes a light, but has a transition of `*`"),
		                            dropped("label", "has an action with no `label`"),
		                            dropped("default", "has slot `n` with a default, which is not a angle"),
		                            dropped("set", "sets `$n` in state `s` to a value of a wrong type"),
		                            dropped("vector", "sets `scale` of `g` in state `s` to a value of a wrong type"),
		                            dropped("two",
		                                    "has a transformation of `g` in state `s`, which does not set a "
		                                    "single channel"),
		                        }));
	}

	class interactions_import : public ::testing::TestWithParam<std::string> {};

	TEST_P(interactions_import, exported_model_comes_back) {
		auto const& format = GetParam();
		auto const path = temp_file(format == "glb2" ? "door.glb" : "door.gltf");
		export_scene("door", door_sink(true), gfx::materials{}, format, path);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();

		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, (parsed{.objects = {door(), light_active_object{.name = "lamp", .light = "150ms"}}}));
	}

	INSTANTIATE_TEST_SUITE_P(formats, interactions_import, ::testing::Values("glb2", "gltf2"));

	// The order of declaration follows the nodes (see Through the sink in
	// the specification): in `A`, the named `B` declares `x` before the
	// unnamed `C` does, but `C` declares it in the node of `A`, opened
	// before the node of `B`, so the `x` of `C` is declared first
	TEST(interactions_export, unnamed_group_declares_before_named_siblings) {
		geo_active_object const from_b{.name = "x", .states = {state("b")}};
		geo_active_object const from_c{.name = "x", .states = {state("c")}};

		gfx::group root{};
		auto& a = root.add_group({.name = "A"});
		a.add_group({.name = "B"}).add_active_object(from_b);
		a.add_group({}).add_active_object(from_c);

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});
		auto const path = temp_file("order.glb");
		export_scene("order", out, gfx::materials{}, "glb2", path);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();
		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, (parsed{.objects = {from_c, from_b}}));
	}

	// Goldilocks: the object describing a light, declared in the group
	// above the light, in the group the light is in, or in a group below it.
	enum class bear { too_shallow, just_right, too_deep };

	std::string bear_name(::testing::TestParamInfo<bear> const& info) {
		switch (info.param) {
			case bear::too_shallow:
				return "too_shallow";
			case bear::just_right:
				return "just_right";
			case bear::too_deep:
				break;
		}
		return "too_deep";
	}

	// `room` > `lamp` > `shade`, each with a cube, and a light in `lamp`;
	// the meshes keep the light from taking the node of `lamp`
	gfx::sink goldilocks_sink(std::string const& light_name, bear placement) {
		auto const box = [] { return gfx::solid::cube(gfx::cm(10), gfx::cm(10), gfx::cm(10)); };
		gfx::group root{};
		auto& room = root.add_group({.name = "room"});
		room.add<gfx::solid_object>(box());
		auto& lamp = room.add_group({.name = "lamp"});
		lamp.add<gfx::solid_object>(box());
		lamp.add(gfx::light{.name = light_name});
		auto& shade = lamp.add_group({.name = "shade"});
		shade.add<gfx::solid_object>(box());

		auto& owner = placement == bear::too_shallow ? room : placement == bear::just_right ? lamp : shade;
		owner.set_light_transition(light_name, "1s");

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});
		return out;
	}

	class goldilocks : public ::testing::TestWithParam<bear> {
	protected:
		// the active objects read back from the exported model
		active_objects round_trip(gfx::sink const& out, rapidjson::Document* doc = nullptr) {
			auto const path = temp_file("lamp.glb");
			export_scene("lamp", out, gfx::materials{}, "glb2", path);
			if (doc) *doc = read_document(path);

			Assimp::Importer importer{};
			gltf2::register_importer(importer);
			auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
			if (!scene) {
				ADD_FAILURE() << importer.GetErrorString();
				return {};
			}
			auto const* result = gltf2::interactions_of(importer, scene);
			if (!result) {
				ADD_FAILURE() << "no interactions";
				return {};
			}
			EXPECT_EQ(result->notes, std::vector<note>{});
			return result->objects;
		}
	};

	// `bulb` is written on a node of its own name, so the importer finds it,
	// wherever its object was declared
	TEST_P(goldilocks, properly_named_light_survives) {
		rapidjson::Document doc{};
		EXPECT_EQ(round_trip(goldilocks_sink("bulb", GetParam()), &doc),
		          (active_objects{light_active_object{.name = "bulb", .light = "1s"}}));
		EXPECT_TRUE(has_node(doc, "bulb"));
	}

	// `lamp` is the name of the group with meshes, so the light is written
	// as `Light`; only the object declared in the group of the light follows
	// it, the others are dropped
	TEST_P(goldilocks, generated_name_survives_just_right) {
		auto const objects = round_trip(goldilocks_sink("lamp", GetParam()));
		if (GetParam() == bear::just_right) {
			EXPECT_EQ(objects, (active_objects{light_active_object{.name = "Light", .light = "1s"}}));
		} else {
			EXPECT_EQ(objects, active_objects{});
		}
	}

	INSTANTIATE_TEST_SUITE_P(bears,
	                         goldilocks,
	                         ::testing::Values(bear::too_shallow, bear::just_right, bear::too_deep),
	                         bear_name);

	// the light `bulb`, alone in the group `lamp`, does not take the node of
	// the group, so an object moving `lamp` comes back
	TEST(interactions_import, group_holding_a_light_of_another_name_comes_back) {
		gfx::group root{};
		auto& lamp = root.add_group({.name = "lamp"});
		lamp.add(gfx::light{.name = "bulb"});
		geo_active_object const swing{
		    .name = "swing",
		    .states = {state("still"), state("turned").transform("lamp", channel::rotate_y(rad(1)))},
		    .actions = {{"Turn", "still", "turned"}, {"Return", "turned", "still"}},
		};
		root.add_active_object(swing);
		root.set_light_transition("bulb", "1s");

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});
		auto const path = temp_file("lamp.glb");
		export_scene("lamp", out, gfx::materials{}, "glb2", path);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();
		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, (parsed{.objects = {swing, light_active_object{.name = "bulb", .light = "1s"}}}));
	}

	// two lights with no name, each in a group of the shared name `a`, are
	// written on generated nodes, `Light` and `Light-001`; on import, these
	// are the names of the lights, so they must keep the naming rules, or
	// every model with them would be read with a warning
	TEST(interactions_import, generated_light_names_keep_the_rules) {
		gfx::group root{};
		root.add_group({.name = "a"}).add(gfx::light{});
		root.add_group({.name = "a"}).add(gfx::light{});
		root.add_group({.name = "lamp"}).add(gfx::light{.name = "lamp"});
		root.set_light_transition("lamp", "1s");

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});
		auto const path = temp_file("lamp.glb");
		export_scene("lamp", out, gfx::materials{}, "glb2", path);
		auto const doc = read_document(path);
		EXPECT_TRUE(has_node(doc, "Light"));
		EXPECT_TRUE(has_node(doc, "Light-001"));

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();
		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, (parsed{.objects = {light_active_object{.name = "lamp", .light = "1s"}}}));
	}

	// two lights of one name still fail the export, even though they are
	// written on nodes of their own
	TEST(interactions_export, lights_sharing_a_name_fail) {
		gfx::group root{};
		root.add_group({.name = "left"}).add(gfx::light{.name = "bulb"});
		root.add_group({.name = "right"}).add(gfx::light{.name = "bulb"});
		root.set_light_transition("bulb", "1s");

		gfx::sink out{};
		root.output(out, glm::dmat4{1.0});
		EXPECT_THROW(export_scene("lamp", out, gfx::materials{}, "glb2", temp_file("lamp.glb")), std::runtime_error);
	}

	// the extras of every node, in the order of the `nodes` array; the
	// punctual light of node 3 is a light of the model
	TEST(interactions_import, extras_of_all_nodes_are_merged) {
		auto const path = temp_file("merged.gltf");
		write_text(path, R"({
	"asset": {"version": "2.0"},
	"extensionsUsed": ["KHR_lights_punctual"],
	"extensions": {"KHR_lights_punctual": {"lights": [{"type": "point"}]}},
	"scenes": [{"nodes": [2]}],
	"nodes": [
		{"name": "first", "extras": {"PYXEL_interactions": [{"name": "a", "version": "1.0"}]}},
		{"name": "broken", "extras": {"PYXEL_interactions": {"name": "b", "version": "1.0"}}},
		{"name": "root", "children": [0, 1, 3],
		 "extras": {"PYXEL_interactions": [{"name": "c", "version": "1.0"}, {"name": "d", "version": "2.0"}]}},
		{"name": "bulb", "extensions": {"KHR_lights_punctual": {"light": 0}},
		 "extras": {"PYXEL_interactions": [{"name": "bulb", "version": "1.0", "transitions": {"light": "1s"}}]}}
	]
})");

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();

		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(*result, (parsed{
		                       .objects =
		                           {
		                               geo_active_object{.name = "a"},
		                               geo_active_object{.name = "c"},
		                               light_active_object{.name = "bulb", .light = "1s"},
		                           },
		                       .notes = {dropped("d", "version 2.0 is not supported")},
		                   }));
	}

	// Two nodes, both `lamp`, hold a light each, so the lights share a
	// name; the other objects break one rule or two, but `door`
	TEST(interactions_import, faulty_objects_are_dropped) {
		auto const path = temp_file("faulty.gltf");
		write_text(path, R"({
	"asset": {"version": "2.0"},
	"extensionsUsed": ["KHR_lights_punctual"],
	"extensions": {"KHR_lights_punctual": {"lights": [{"type": "point"}, {"type": "point"}]}},
	"scenes": [{"nodes": [0]}],
	"nodes": [
		{"name": "root", "children": [1, 2, 3], "extras": {"PYXEL_interactions": [
			{"name": "door", "version": "1.0",
			 "states": {"closed": {}, "opened": {"hinge": [{"rotate.y": 1}]}},
			 "actions": [{"label": "Open", "when": "closed", "then": "opened"}]},
			{"name": "broken", "version": "1.0",
			 "states": {"s": {"nowhere": [{"rotate.y": 1}]}},
			 "actions": [{"label": "Go", "when": "s", "then": "lost"}]},
			{"name": "lamp", "version": "1.0", "transitions": {"light": "1s"}},
			{"name": "slow", "version": "1.0", "transitions": {"*": "forever"}}
		]}},
		{"name": "hinge"},
		{"name": "lamp", "extensions": {"KHR_lights_punctual": {"light": 0}}},
		{"name": "lamp", "extensions": {"KHR_lights_punctual": {"light": 1}}}
	]
})");

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();

		auto const* result = gltf2::interactions_of(importer, scene);
		ASSERT_NE(result, nullptr);
		EXPECT_EQ(result->objects,
		          (active_objects{geo_active_object{
		              .name = "door",
		              .states = {state("closed"), state("opened").transform("hinge", channel::rotate_y(rad(1)))},
		              .actions = {{"Open", "closed", "opened"}},
		          }}));
		EXPECT_EQ(result->notes,
		          (std::vector<note>{
		              dropped("broken",
		                      "names group `nowhere` in state `s`, which is not a node of the model; has action `Go` "
		                      "leading to state `lost`, which it does not declare"),
		              dropped("lamp", "describes a light, whose name is not unique"),
		              dropped("slow",
		                      "has a default transition, which has `forever`, which is not a channel, a "
		                      "timing function or a time"),
		          }));
	}

	TEST(interactions_import, other_scenes_have_none) {
		auto const path = temp_file("door.obj");
		export_scene("door", door_sink(false), gfx::materials{}, "obj", path);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		auto const* scene = importer.ReadFile(as_str(path.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();
		EXPECT_EQ(gltf2::interactions_of(importer, scene), nullptr);

		Assimp::Importer plain{};
		EXPECT_EQ(gltf2::interactions_of(plain, nullptr), nullptr);
	}

	// a scene of another file, read after the glTF 2 one, may take the
	// address of the freed glTF 2 scene; here, a new scene is built in the
	// place of the glTF 2 scene by hand, as the allocator may not reuse it
	TEST(interactions_import, later_scene_at_the_same_address_has_none) {
		auto const path = temp_file("door.glb");
		export_scene("door", door_sink(true), gfx::materials{}, "glb2", path);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		ASSERT_NE(importer.ReadFile(as_str(path.u8string()), 0u), nullptr) << importer.GetErrorString();

		std::unique_ptr<aiScene> scene{importer.GetOrphanedScene()};
		ASSERT_NE(gltf2::interactions_of(importer, scene.get()), nullptr);

		scene->~aiScene();
		new (scene.get()) aiScene{};
		EXPECT_EQ(gltf2::interactions_of(importer, scene.get()), nullptr);
	}

	TEST(interactions_import, next_file_of_the_same_importer_has_none) {
		auto const glb = temp_file("door.glb");
		auto const obj = temp_file("door.obj");
		export_scene("door", door_sink(true), gfx::materials{}, "glb2", glb);
		export_scene("door", door_sink(false), gfx::materials{}, "obj", obj);

		Assimp::Importer importer{};
		gltf2::register_importer(importer);
		ASSERT_NE(importer.ReadFile(as_str(glb.u8string()), 0u), nullptr) << importer.GetErrorString();
		auto const* scene = importer.ReadFile(as_str(obj.u8string()), 0u);
		ASSERT_NE(scene, nullptr) << importer.GetErrorString();
		EXPECT_EQ(gltf2::interactions_of(importer, scene), nullptr);
	}
}  // namespace pyxel::io::testing
