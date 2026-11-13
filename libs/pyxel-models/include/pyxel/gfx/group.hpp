// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <pyxel/gfx/object.hpp>
#include <pyxel/gfx/transform.hpp>
#include <string>
#include <utility>
#include <vector>

namespace pyxel::gfx {
	// named, painted and positioned container of other objects; all the
	// setters return the group itself, so the calls can be chained
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

		struct add_group_options {
			offset pos{};
			std::string name{};
			std::string material{};
		};
		group& add_group(add_group_options const& options) {
			return add<group>(options.pos).set_name(options.name).set_material(options.material);
		}

		std::vector<std::unique_ptr<object>> const& children() const noexcept { return children_; }

		void output(sink&, glm::dmat4 const& current) const override;

	private:
		std::string name_{};
		std::string material_{};
		offset position_{};
		glm::dmat4 transform_{1};
		std::vector<std::unique_ptr<object>> children_{};
	};

}  // namespace pyxel::gfx
