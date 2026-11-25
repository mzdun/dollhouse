// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/iostream.h>
#include <pybind11/stl.h>
#include <format>
#include <iostream>
#include <shapes/__main__/main.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include <vector>

// `shapes.__main__`, the module run by `python -m shapes`, comes from the
// extension itself: a finder on sys.meta_path gives its spec, and the loader
// gives the code of a `__main__.py` holding
//
//     sys.exit(main(sys.argv))
//
// runpy executes that code in the `__main__` namespace; a plain import of
// `shapes.__main__` executes it in the new module, so it exits as well.
//
// The module is never put into sys.modules up front, since runpy warns about
// a `__main__` imported before it is run.

namespace shapes::glue {
	namespace {
		constexpr auto main_name = "shapes.__main__";

		constexpr auto main_source =
		    "import sys\n"
		    "main = __loader__.main\n"
		    "sys.exit(main(sys.argv))\n";

		// both the finder and the loader of `shapes.__main__`
		struct main_importer {
			static int main(std::vector<std::string>& argv) {
				// std::cout and std::cerr follow sys.stdout and sys.stderr
				py::scoped_ostream_redirect const out{std::cout, py::module_::import("sys").attr("stdout")};
				py::scoped_estream_redirect const err{std::cerr, py::module_::import("sys").attr("stderr")};
				return shapes::__main__::main(argv);
			}

			// finder
			py::object find_spec(std::string const& fullname, py::object const&, py::object const&) const {
				if (fullname != main_name) return py::none();

				// the extension itself; sys.argv[0] becomes this path under `-m`
				auto const origin = py::module_::import("shapes").attr("__file__");
				auto const machinery = py::module_::import("importlib.machinery");
				return machinery.attr("ModuleSpec")(main_name, py::cast(this, py::return_value_policy::reference),
				                                    "origin"_a = origin);
			}

			// loader
			py::object create_module(py::object const&) const { return py::none(); }

			void exec_module(py::object const& module) const {
				auto const code = get_code(main_name);
				py::module_::import("builtins").attr("exec")(code, module.attr("__dict__"));
			}

			py::object get_code(std::string const& fullname) const {
				if (fullname != main_name) {
					throw py::import_error(std::format("shapes: cannot load `{}`", fullname));
				}
				return py::module_::import("builtins").attr("compile")(main_source, "<shapes.__main__>", "exec");
			}

			py::object get_source(std::string const& fullname) const {
				if (fullname != main_name) return py::none();
				return py::str(main_source);
			}

			bool is_package(std::string const&) const { return false; }
		};
	}  // namespace

	void bind_main(py::module_& m) {
		auto importer = py::class_<main_importer>(
		    m, "_main_importer",
		    "Finder and loader of `shapes.__main__`, the module run by `python -m shapes`. Installed on "
		    "`sys.meta_path` when `shapes` is imported.");
		importer.def(py::init<>())
		    .def_static("main", &main_importer::main, "argv"_a,
		                "Runs the shapes command-line tool; returns the exit code.\n"
		                "\n"
		                ":param argv: Command line, as in ``sys.argv``.")
		    .def("find_spec", &main_importer::find_spec, "fullname"_a, "path"_a = py::none(), "target"_a = py::none())
		    .def("create_module", &main_importer::create_module, "spec"_a)
		    .def("exec_module", &main_importer::exec_module, "module"_a)
		    .def("get_code", &main_importer::get_code, "fullname"_a)
		    .def("get_source", &main_importer::get_source, "fullname"_a)
		    .def("is_package", &main_importer::is_package, "fullname"_a);

		// once per interpreter, even if the extension is initialized again
		auto meta_path = py::module_::import("sys").attr("meta_path").cast<py::list>();
		for (auto const& finder : meta_path) {
			if (py::isinstance(finder, importer)) return;
		}
		meta_path.append(importer());
	}
}  // namespace shapes::glue
