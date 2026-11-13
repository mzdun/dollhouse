// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <map>
#include <pyxel/gfx/solid.hpp>
#include <pyxel/math/glm.hpp>
#include <string>
#include <vector>

namespace pyxel::gfx {
	struct sink {
		void add_solid(solid const& solid, glm::dmat4 const& matrix);

		void push_object_name(std::string const& name);
		void pop_object_name();

		void push_material(std::string const& name);
		void pop_material();

		// pushes the non-empty name and material for the lifetime of the
		// guard, so they are popped even if a child throws
		class scope_context {
		public:
			scope_context(sink& out, std::string const& name, std::string const& material)
			    : out_{out}, has_name_{!name.empty()}, has_material_{!material.empty()} {
				if (has_name_) out_.push_object_name(name);
				if (has_material_) out_.push_material(material);
			}

			~scope_context() {
				if (has_material_) out_.pop_material();
				if (has_name_) out_.pop_object_name();
			}

			scope_context(scope_context const&) = delete;
			scope_context& operator=(scope_context const&) = delete;

		private:
			sink& out_;
			bool has_name_;
			bool has_material_;
		};

		struct glm_solid {
			using face_t = std::vector<size_t>;
			using faces_t = std::vector<face_t>;

			std::vector<glm::dvec3> const& vertices() const noexcept { return vertices_; }
			faces_t const& faces() const noexcept { return faces_; }
			faces_t& faces() noexcept { return faces_; }

			void add_solid(solid const& obj, glm::dmat4 const& matrix);

		private:
			struct dvec3_less {
				bool operator()(glm::dvec3 const& lhs, glm::dvec3 const& rhs) const noexcept {
					if (lhs.x != rhs.x) return lhs.x < rhs.x;
					if (lhs.y != rhs.y) return lhs.y < rhs.y;
					return lhs.z < rhs.z;
				}
			};

			size_t add_vertex(glm::dvec3 const& vertex);

			std::vector<glm::dvec3> vertices_{};
			std::map<glm::dvec3, size_t, dvec3_less> lookup_{};
			faces_t faces_{};
		};

		using strings = std::vector<std::string>;
		using solids_t = std::map<std::string, std::map<strings, glm_solid>>;

		[[nodiscard]] scope_context scope(std::string const& object_name = {}, std::string const& material = {}) {
			return {*this, object_name, material};
		}

		solids_t const& solids() const noexcept { return solids_; }
		solids_t& solids() noexcept { return solids_; }

	private:
		strings names_{};
		strings materials_{};
		solids_t solids_{};
	};
}  // namespace pyxel::gfx
