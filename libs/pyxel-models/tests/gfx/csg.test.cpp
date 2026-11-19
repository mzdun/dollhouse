// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <pyxel/gfx.hpp>
#include <pyxel/gfx/csg.hpp>
#include <pyxel/math/glm.hpp>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace pyxel::gfx::testing {
	using namespace math::literals;

	// axis-aligned box; counter-clockwise faces, when seen from outside
	solid box(offset const& min, offset const& max) {
		solid result{};
		size_t v[8]{};
		for (size_t index = 0; index < 8; ++index) {
			v[index] = result.add_vert(index & 4 ? max.x : min.x, index & 2 ? max.y : min.y, index & 1 ? max.z : min.z);
		}
		result.add_face(v[0], v[1], v[3], v[2]);
		result.add_face(v[4], v[6], v[7], v[5]);
		result.add_face(v[0], v[4], v[5], v[1]);
		result.add_face(v[2], v[3], v[7], v[6]);
		result.add_face(v[0], v[2], v[6], v[4]);
		result.add_face(v[1], v[5], v[7], v[3]);
		return result;
	}

	solid cube_at(offset const& min, length edge) { return box(min, min + offset{edge, edge, edge}); }

	// divergence theorem; positive for closed solids with outward faces, in m^3
	double volume_of(solid const& obj) {
		auto const& vertices = obj.vertices();
		double result = 0.0;
		for (auto const& face : obj.faces()) {
			auto const origin = vertices[face[0]].to_glm();
			for (size_t index = 1; index + 1 < face.size(); ++index) {
				result +=
				    glm::dot(origin, glm::cross(vertices[face[index]].to_glm(), vertices[face[index + 1]].to_glm()));
			}
		}
		return result / 6.0;
	}

	offset min_of(solid const& obj) {
		auto result = obj.vertices().front();
		for (auto const& vertex : obj.vertices())
			result = min_pt(result, vertex);
		return result;
	}

	offset max_of(solid const& obj) {
		auto result = obj.vertices().front();
		for (auto const& vertex : obj.vertices())
			result = max_pt(result, vertex);
		return result;
	}

	constexpr double liter = 1e-3;  // m^3, a 10 cm cube
	constexpr double tolerance = 1e-12;

	// two 10 cm cubes, the second moved by 5 cm along x, so they share half
	// of their volume
	solid const A = cube_at(offset{}, 10_cm);
	solid const B = cube_at(offset::at_x(5_cm), 10_cm);

	TEST(gfx_csg, box_helper_is_outward) {
		EXPECT_NEAR(volume_of(A), liter, tolerance);
		EXPECT_NEAR(volume_of(B), liter, tolerance);
	}

	TEST(gfx_csg, unite_overlapping) {
		auto const result = unite(A, B);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), 1.5 * liter, tolerance);
		EXPECT_TRUE(near(min_of(result), offset{}));
		EXPECT_TRUE(near(max_of(result), offset{15_cm, 10_cm, 10_cm}));
	}

	TEST(gfx_csg, intersect_overlapping) {
		auto const result = intersect(A, B);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), .5 * liter, tolerance);
		EXPECT_TRUE(near(min_of(result), offset::at_x(5_cm)));
		EXPECT_TRUE(near(max_of(result), offset{10_cm, 10_cm, 10_cm}));
	}

	TEST(gfx_csg, subtract_overlapping) {
		auto const result = subtract(A, B);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), .5 * liter, tolerance);
		EXPECT_TRUE(near(min_of(result), offset{}));
		EXPECT_TRUE(near(max_of(result), offset{5_cm, 10_cm, 10_cm}));

		auto const other_way = subtract(B, A);
		EXPECT_NEAR(volume_of(other_way), .5 * liter, tolerance);
		EXPECT_TRUE(near(min_of(other_way), offset::at_x(10_cm)));
		EXPECT_TRUE(near(max_of(other_way), offset{15_cm, 10_cm, 10_cm}));
	}

	TEST(gfx_csg, symmetric_difference_overlapping) {
		auto const result = symmetric_difference(A, B);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), liter, tolerance);
		EXPECT_TRUE(near(min_of(result), offset{}));
		EXPECT_TRUE(near(max_of(result), offset{15_cm, 10_cm, 10_cm}));
	}

	TEST(gfx_csg, operands_are_not_changed) {
		auto const copy = A;
		[[maybe_unused]] auto const result = subtract(A, B);
		EXPECT_EQ(A.vertices(), copy.vertices());
		EXPECT_EQ(A.faces(), copy.faces());
	}

	TEST(gfx_csg, disjoint_solids_are_not_cut) {
		// apart on Z, but the top of `low` (y = 10 cm) crosses the sides of
		// `high`; going through the trees, the sides would be cut in two (the
		// Z planes come last in the tree of a box, so the cut comes first)
		auto const low = cube_at(offset{}, 10_cm);
		auto const high = cube_at(offset{0_m, 5_cm, 1_m}, 10_cm);
		EXPECT_EQ(unite(low, high).faces().size(), 12u);
		EXPECT_EQ(subtract(low, high).faces().size(), 6u);
		EXPECT_EQ(subtract(high, low).faces().size(), 6u);
		EXPECT_EQ(symmetric_difference(low, high).faces().size(), 12u);
	}

	TEST(gfx_csg, disjoint_solids) {
		auto const far = cube_at(offset::at_x(1.0_m), 10_cm);
		EXPECT_NEAR(volume_of(unite(A, far)), 2 * liter, tolerance);
		EXPECT_TRUE(intersect(A, far).faces().empty());
		EXPECT_NEAR(volume_of(subtract(A, far)), liter, tolerance);
		EXPECT_NEAR(volume_of(symmetric_difference(A, far)), 2 * liter, tolerance);
	}

	TEST(gfx_csg, nested_solids) {
		// 20 cm cube with A in its corner, 5 cm inside on every axis
		auto const outer = cube_at(offset{-5_cm, -5_cm, -5_cm}, 20_cm);
		EXPECT_NEAR(volume_of(unite(outer, A)), 8 * liter, tolerance);
		EXPECT_NEAR(volume_of(intersect(outer, A)), liter, tolerance);
		EXPECT_TRUE(subtract(A, outer).faces().empty());

		// a hollow box: the cavity keeps its faces, now pointing inwards
		auto const hollow = subtract(outer, A);
		EXPECT_NEAR(volume_of(hollow), 7 * liter, tolerance);
		EXPECT_NEAR(volume_of(symmetric_difference(outer, A)), 7 * liter, tolerance);
	}

	TEST(gfx_csg, solids_touching_at_a_face) {
		auto const next = cube_at(offset::at_x(10_cm), 10_cm);
		EXPECT_NEAR(volume_of(unite(A, next)), 2 * liter, tolerance);
		EXPECT_NEAR(std::abs(volume_of(intersect(A, next))), 0.0, tolerance);
		EXPECT_NEAR(volume_of(subtract(A, next)), liter, tolerance);
	}

	TEST(gfx_csg, same_solid_twice) {
		EXPECT_NEAR(volume_of(unite(A, A)), liter, tolerance);
		EXPECT_NEAR(volume_of(intersect(A, A)), liter, tolerance);
		EXPECT_NEAR(std::abs(volume_of(subtract(A, A))), 0.0, tolerance);
		EXPECT_NEAR(std::abs(volume_of(symmetric_difference(A, A))), 0.0, tolerance);
	}

	TEST(gfx_csg, empty_operands) {
		solid const empty{};
		EXPECT_NEAR(volume_of(unite(A, empty)), liter, tolerance);
		EXPECT_NEAR(volume_of(unite(empty, A)), liter, tolerance);
		EXPECT_TRUE(intersect(A, empty).faces().empty());
		EXPECT_NEAR(volume_of(subtract(A, empty)), liter, tolerance);
		EXPECT_TRUE(subtract(empty, A).faces().empty());
		EXPECT_NEAR(volume_of(symmetric_difference(empty, A)), liter, tolerance);
	}

	TEST(gfx_csg, broken_faces_are_ignored) {
		auto broken = A;
		broken.add_face(std::vector<size_t>{0, 1});       // too short
		broken.add_face(std::vector<size_t>{0, 1, 100});  // index past the last vertex
		broken.add_face(std::vector<size_t>{0, 0, 0});    // no area
		EXPECT_NEAR(volume_of(unite(broken, B)), 1.5 * liter, tolerance);
	}

	// ---- cleanup ---------------------------------------------------------

	size_t used_vertices(solid const& obj) {
		std::vector<bool> used(obj.vertices().size(), false);
		for (auto const& face : obj.faces())
			for (auto const index : face)
				used[index] = true;
		return static_cast<size_t>(std::ranges::count(used, true));
	}

	TEST(gfx_csg_cleanup, keeps_a_clean_solid) {
		auto const result = cleanup(A);
		EXPECT_EQ(result.vertices().size(), 8u);
		EXPECT_EQ(result.faces().size(), 6u);
		EXPECT_NEAR(volume_of(result), liter, tolerance);
	}

	TEST(gfx_csg_cleanup, corner_overlap) {
		// two 1 m cubes, the second moved by half a meter along every axis
		auto const lhs = cube_at(offset{}, 1_m);
		auto const rhs = cube_at(offset{50_cm, 50_cm, 50_cm}, 1_m);
		auto const raw = unite(lhs, rhs);
		ASSERT_GT(raw.vertices().size(), 20u);

		// 7 + 7 outer corners, 3 + 3 edges crossing faces; 3 + 3 outer
		// squares, 3 L-shapes on the planes x, y, z = 1 m and 3 more on the
		// planes x, y, z = 0.5 m; V - E + F = 20 - 30 + 12 = 2
		auto const result = cleanup(raw);
		ASSERT_TRUE(result.is_valid());
		EXPECT_EQ(result.vertices().size(), 20u);
		EXPECT_EQ(used_vertices(result), 20u);
		EXPECT_EQ(result.faces().size(), 12u);
		EXPECT_NEAR(volume_of(result), volume_of(raw), 1e-9);
		EXPECT_NEAR(volume_of(result), 2 - .125, 1e-9);
	}

	TEST(gfx_csg_cleanup, every_operation) {
		auto const check = [](solid const& raw, size_t vertices, size_t faces) {
			auto const result = cleanup(raw);
			EXPECT_TRUE(result.is_valid());
			EXPECT_EQ(result.vertices().size(), vertices);
			EXPECT_EQ(result.faces().size(), faces);
			EXPECT_NEAR(volume_of(result), volume_of(raw), tolerance);
		};
		// A and B overlap by half along x: boxes come out as boxes
		check(unite(A, B), 8, 6);
		check(intersect(A, B), 8, 6);
		check(subtract(A, B), 8, 6);
		// two separate boxes, [0, 5] and [10, 15] along x
		check(symmetric_difference(A, B), 16, 12);
	}

	TEST(gfx_csg_cleanup, faces_with_holes_are_not_merged) {
		// a 30 cm slab with a 10 cm square hole through its middle
		auto const slab = box(offset{}, offset{30_cm, 10_cm, 30_cm});
		auto const hole = box(offset{10_cm, -1_cm, 10_cm}, offset{20_cm, 11_cm, 20_cm});
		auto const raw = subtract(slab, hole);
		auto const result = cleanup(raw);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), 8 * liter, tolerance);
		// the outer and the hole's sides are merged, the holed top and
		// bottom are not; nothing is left in the middle of a straight edge
		EXPECT_LE(result.faces().size(), raw.faces().size());
		EXPECT_EQ(used_vertices(result), result.vertices().size());
	}

	TEST(gfx_csg_cleanup, is_idempotent) {
		auto const once = cleanup(unite(cube_at(offset{}, 1_m), cube_at(offset{50_cm, 50_cm, 50_cm}, 1_m)));
		auto const twice = cleanup(once);
		EXPECT_EQ(twice.vertices().size(), once.vertices().size());
		EXPECT_EQ(twice.faces().size(), once.faces().size());
		EXPECT_NEAR(volume_of(twice), volume_of(once), 1e-9);
	}

	TEST(gfx_csg_cleanup, several_operations_then_one_cleanup) {
		// a 30 cm bar, made of three cubes in a row, with the middle third
		// of a smaller cube cut out of its top
		auto const bar = unite(unite(cube_at(offset{}, 10_cm), cube_at(offset::at_x(10_cm), 10_cm)),
		                       cube_at(offset::at_x(20_cm), 10_cm));
		auto const notched = subtract(bar, box(offset{10_cm, 5_cm, -1_cm}, offset{20_cm, 11_cm, 11_cm}));
		auto const result = cleanup(notched);
		ASSERT_TRUE(result.is_valid());
		EXPECT_NEAR(volume_of(result), 2.5 * liter, tolerance);
		// a U-shaped prism: two U faces, and 8 rectangles around them
		EXPECT_EQ(result.faces().size(), 10u);
		EXPECT_EQ(result.vertices().size(), 16u);
	}

	TEST(gfx_csg_cleanup, empty_stays_empty) {
		auto const result = cleanup(solid{});
		EXPECT_TRUE(result.vertices().empty());
		EXPECT_TRUE(result.faces().empty());
	}

	// ---- deep trees ------------------------------------------------------------

	namespace {
		// runs the function on a new thread with the given stack size; an
		// exception thrown by it is rethrown here; a stack overflow crashes
		void run_on_stack(size_t stack_size, std::function<void()> const& fn) {
			struct call {
				std::function<void()> const& fn;
				std::exception_ptr error{};

				void run() noexcept {
					try {
						fn();
					} catch (...) {
						error = std::current_exception();
					}
				}
			} context{fn};

#ifdef _WIN32
			auto const thread = CreateThread(
			    nullptr, stack_size,
			    [](void* arg) -> DWORD {
				    static_cast<call*>(arg)->run();
				    return 0;
			    },
			    &context, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
			ASSERT_NE(thread, nullptr);
			WaitForSingleObject(thread, INFINITE);
			CloseHandle(thread);
#else
			pthread_attr_t attr{};
			ASSERT_EQ(pthread_attr_init(&attr), 0);
			ASSERT_EQ(pthread_attr_setstacksize(&attr, stack_size), 0);
			pthread_t thread{};
			auto const created = pthread_create(
			    &thread, &attr,
			    [](void* arg) -> void* {
				    static_cast<call*>(arg)->run();
				    return nullptr;
			    },
			    &context);
			pthread_attr_destroy(&attr);
			ASSERT_EQ(created, 0);
			pthread_join(thread, nullptr);
#endif
			if (context.error) std::rethrow_exception(context.error);
		}

		// a convex solid makes a BSP tree as deep as it has faces; with the
		// recursive tree walks, 300 sides overflowed a 64 KiB stack
		constexpr size_t deep_sides = 300;
		constexpr size_t small_stack = 64 * 1024;
	}  // namespace

	TEST(gfx_csg_deep, convex_solid_on_a_small_stack) {
		auto const dense = solid::prism(10_cm, 10_cm, deep_sides);
		// half in, half out of the top of the prism, so the bounding boxes
		// overlap and every operation goes through the trees
		auto const cap = cube_at(offset::at_y(9.5_cm), 1_cm);
		auto const dense_volume = volume_of(dense);
		auto const half = 0.5e-6;

		run_on_stack(small_stack, [&] {
			EXPECT_NEAR(volume_of(unite(dense, cap)), dense_volume + half, 1e-9);
			EXPECT_NEAR(volume_of(subtract(dense, cap)), dense_volume - half, 1e-9);
			EXPECT_NEAR(volume_of(intersect(dense, cap)), half, 1e-9);
			EXPECT_NEAR(volume_of(symmetric_difference(dense, cap)), dense_volume, 1e-9);
		});
	}
}  // namespace pyxel::gfx::testing
