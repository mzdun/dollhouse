// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pyxel/gfx/solid.hpp>

// Constructive solid geometry: set operations on the volumes enclosed by
// solids.
//
// The operands must be closed (watertight) polyhedra, with convex, planar
// faces, wound counter-clockwise, when looked at from the outside. Faces
// with fewer than three vertices, with indices past the last vertex, or with
// no area are ignored.
//
// The results satisfy the same conditions, but faces of the operands may be
// split into several smaller ones, and new vertices may lie on edges of
// neighbouring faces (T-junctions). An empty result is a solid with no
// faces.
namespace pyxel::gfx {
	// A | B: everything inside A, or B, or both
	solid unite(solid const& lhs, solid const& rhs);

	// A & B: everything inside both A and B
	solid intersect(solid const& lhs, solid const& rhs);

	// A - B: everything inside A, but not inside B
	solid subtract(solid const& lhs, solid const& rhs);

	// A ^ B: everything inside exactly one of A and B
	solid symmetric_difference(solid const& lhs, solid const& rhs);

	// Tidies up the result of one or more of the operations above, without
	// changing the volume:
	//
	// - vertices closer than 0.1 um are merged,
	// - T-junctions are removed, by adding the vertices lying on an edge to
	//   the faces along that edge,
	// - neighbouring faces lying in the same plane are merged into one,
	//   unless the merged face would have a hole,
	// - vertices in the middle of a straight edge are removed, unless they
	//   are a corner of another face.
	//
	// The merged faces may be concave, so the result is meant for output
	// only: after cleanup, no operations are allowed, i.e. the result must not
	// be passed to unite, intersect, subtract or symmetric_difference. Do all
	// the operations first, then clean up once.
	solid cleanup(solid const& obj);
}  // namespace pyxel::gfx
