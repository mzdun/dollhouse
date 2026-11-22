// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <pyxel/gfx/interactions.hpp>
#include <pyxel/gfx/light_object.hpp>
#include <pyxel/gfx/object.hpp>
#include <pyxel/gfx/transform.hpp>
#include <string>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	// named, painted and positioned container of other objects; all the
	// setters return the group itself, so the calls can be chained.
	//
	// A named group is a node of the sink: its transformation, together with
	// the ones of the unnamed groups above it (up to the nearest named
	// group), is kept in the node, and its children start from identity.
	// The transformation of an unnamed group is passed down instead, and
	// baked into the vertices. So the transformation reaching a named group
	// must be made of a translation, a rotation and a scale only: no shear,
	// e.g. no non-uniform scale after a rotation (see sink). A negative
	// scale, mirroring the contents, is fine
	struct group : object {
		group(length x = {}, length y = {}, length z = {}) : group{offset{x, y, z}} {}
		group(offset const& pos) : transform_{gfx::translate(identity_matrix, pos)} {}

		// name and material
		std::string const& name() const noexcept { return name_; }
		group& set_name(std::string name) {
			name_ = std::move(name);
			return *this;
		}

		std::string const& material() const noexcept { return material_; }
		group& set_material(std::string material) {
			material_ = std::move(material);
			return *this;
		}

		group& transform(glm::dmat4 const& m) noexcept {
			transform_ = m * transform_;
			return *this;
		}
		group& set_transform(glm::dmat4 const& m) noexcept {
			transform_ = m;
			return *this;
		}

		glm::dmat4 const& current_matrix() const noexcept { return transform_; }
		glm::dmat4 current_matrix(glm::dmat4 const& parent) const { return parent * current_matrix(); }

		// children
		template <typename Impl, typename... Args>
		Impl& add(Args&&... args) {
			auto child = std::make_unique<Impl>(std::forward<Args>(args)...);
			auto result = child.get();
			children_.emplace_back(std::move(child));
			return *result;
		}
		object& add(std::unique_ptr<object> child) {
			auto result = child.get();
			children_.emplace_back(std::move(child));
			return *result;
		}
		// adds a copy of the light as a child, wrapped in a light_object
		void add(gfx::light value) { add<light_object>(std::move(value)); }

		struct add_group_options {
			offset pos{};
			std::string name{};
			std::string material{};
		};
		group& add_group(add_group_options const& options) {
			return add<group>(options.pos).set_name(options.name).set_material(options.material);
		}

		std::vector<std::unique_ptr<object>> const& children() const noexcept { return children_; }

		// active objects (see docs/model-interactions.md)

		// Keeps a copy of the declaration of a geometry-based object; nothing
		// is checked here, as the groups it names may be added later.
		group& add_active_object(interactions::geo_active_object declaration) {
			active_objects_.emplace_back(std::move(declaration));
			return *this;
		}

		// The same, with the members of the declaration as arguments, in the
		// order of the structure.
		group& add_active_object(std::string name,
		                         std::vector<interactions::slot> slots = {},
		                         interactions::states states = {},
		                         std::vector<interactions::action> actions = {},
		                         interactions::transitions transitions = {}) {
			return add_active_object(interactions::geo_active_object{
			    .name = std::move(name),
			    .slots = std::move(slots),
			    .states = std::move(states),
			    .actions = std::move(actions),
			    .transitions = std::move(transitions),
			});
		}

		// Declares the transition of the intensity factor of the light
		// `name`, adding an interactions::light_active_object to the list of
		// active objects, or replacing the transition of the one of the same
		// name already declared on this group.
		group& set_light_transition(std::string name, interactions::transition rule);

		// Both kinds of objects, in the order they were added.
		std::vector<interactions::active_object> const& active_objects() const noexcept { return active_objects_; }

		void output(sink&, glm::dmat4 const& current) const override;

	private:
		void add_active_objects(sink& out) const;

		std::string name_{};
		std::string material_{};
		offset position_{};
		glm::dmat4 transform_{1};
		std::vector<std::unique_ptr<object>> children_{};
		std::vector<interactions::active_object> active_objects_{};
	};

}  // namespace pyxel::gfx
