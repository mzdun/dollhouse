// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <format>
#include <optional>
#include <pyxel/gfx/group.hpp>
#include <pyxel/gfx/materials.hpp>
#include <shapes/glue/glue.hpp>
#include <shapes/library.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include "lifetime.hpp"

namespace shapes::glue {
	using pyxel::gfx::color;
	using pyxel::gfx::group;
	using pyxel::gfx::materials;

	namespace {
		void detach_group_wrappers(group const& grp) noexcept {
			detach_wrappers(grp);
			for (auto const& child : grp.children()) {
				if (auto const* child_group = dynamic_cast<group const*>(child.get())) {
					detach_group_wrappers(*child_group);
				}
			}
		}

		// Lends the root group of a model to its model function. The groups
		// belong to the host and outlive the call only until the model is
		// exported; once the call returns, or raises, the Python wrappers of
		// the root and of every group under it are detached, so a group kept
		// by Python raises ReferenceError instead of reaching freed memory.
		class groups_lease {
		public:
			explicit groups_lease(group& root)
			    : root_{root}, py_root_{py::cast(root, py::return_value_policy::reference)} {}
			~groups_lease() { detach_group_wrappers(root_); }

			groups_lease(groups_lease const&) = delete;
			groups_lease& operator=(groups_lease const&) = delete;

			py::object const& py_root() const noexcept { return py_root_; }

		private:
			group& root_;
			py::object py_root_;
		};

		constexpr auto register_paint_doc =
		    "Registers a paint material and returns its name, to use as a `group.material`. Without a `name`, "
		    "the paint is named `Material`, `Material.001`, and so on. Registering the same paint again returns "
		    "the same name.";

		std::string register_paint(library& self,
		                           std::string const& name,
		                           color const& color,
		                           float roughness,
		                           float metallic) {
			return self.register_paint(name, {.color = color, .roughness = roughness, .metallic = metallic});
		}
	}  // namespace

	void bind_library(py::module_& m) {
		static library top_level{};

		// stored models hold Python callables; release them while the
		// interpreter is still alive, not when the static is destroyed
		py::module_::import("atexit").attr("register")(py::cpp_function([] { top_level.clear_models(); }));

		py::class_<library>(m, "library",
		                    "Models of the script, reached only through the static members of this class; models are "
		                    "added with the `@library.model` decorator.")
		    .def_property_readonly_static(
		        "exporters", [](py::object const&) { return library::exporters(); },
		        "Ids of the available export formats, e.g. \"glb2\" or \"obj\".")
		    .def_property_static(
		        "exporter_id",
		        py::cpp_function([](py::object const&) { return library::python_instance().exporter_id(); }),
		        py::cpp_function([](py::object const&, std::string id) {
			        library::python_instance().set_exporter_id(std::move(id));
		        }),
		        "Export format for the models returning None; one of `exporters`, glTF 2 binary (\"glb2\") by "
		        "default.")
		    .def_property_static("output_directory", py::cpp_function([](py::object const&) {
			                         return library::python_instance().output_directory();
		                         }),
		                         py::cpp_function([](py::object const&, std::filesystem::path dir) {
			                         library::python_instance().set_output_directory(std::move(dir));
		                         }),
		                         "Where `build_models` writes the files; the current directory by default.")
		    .def_static(
		        "build_models", [] { return library::python_instance().build_models(); },
		        "Builds every model and writes it to `output_directory`, named after the model, with the "
		        "extension of its export format; returns the paths written. Stops at the first failure.")
		    .def_static(
		        "clear_models",
		        [] {
			        auto& lib = library::python_instance();
			        if (lib.is_building()) {
				        throw std::logic_error("library.clear_models: cannot clear the models while building them");
			        }
			        lib.clear_models();
		        },
		        "Forgets every registered model, built or not, so the names can be registered again. Cannot be "
		        "called while the models are being built.")
		    .def_static(
		        "model",
		        [](py::function fn) -> py::function {
			        auto const name = fn.attr("__name__").cast<std::string>();

			        try {
				        py::module_::import("inspect").attr("signature")(fn).attr("bind")(py::none());
			        } catch (py::error_already_set& err) {
				        if (!err.matches(PyExc_TypeError)) throw;
				        throw py::type_error(std::format(
				            "@library.model: `{}` must be callable with a single argument, the root group", name));
			        }

			        auto added = library::python_instance().add_model(
			            name, [fn, name](group& root) -> std::optional<std::string> {
				            py::gil_scoped_acquire gil{};
				            // the root belongs to the host; Python only borrows it
				            auto const result = [&] {
					            groups_lease const lease{root};
					            return fn(lease.py_root());
				            }();
				            if (result.is_none()) return std::nullopt;
				            if (!py::isinstance<py::str>(result)) {
					            throw py::type_error(
					                std::format("@library.model: `{}` must return an exporter id (str) or None, not {}",
					                            name, py::type::of(result).attr("__name__").cast<std::string>()));
				            }
				            return result.cast<std::string>();
			            });
			        if (!added) {
				        throw py::value_error(std::format("@library.model: `{}` is already registered", name));
			        }

			        return fn;
		        },
		        py::arg("fn"),
		        "Registers a model, named after the function. The function is called with the root `group` to "
		        "fill, and returns the id of the exporter to use for the model, or None for the default one.")

		    .def_static(
		        "register_paint",
		        [](color const& color, float roughness, float metallic) {
			        return register_paint(library::python_instance(), {}, color, roughness, metallic);
		        },
		        "color"_a, "roughness"_a = 1.0f, "metallic"_a = 0.0f, "Registers the paint with the library in use.")
		    .def_static(
		        "register_paint",
		        [](std::string const& name, color const& color, float roughness, float metallic) {
			        return register_paint(library::python_instance(), name, color, roughness, metallic);
		        },
		        "name"_a, "color"_a, "roughness"_a = 1.0f, "metallic"_a = 0.0f,
		        "Registers the paint with the library in use.");
	}
}  // namespace shapes::glue
