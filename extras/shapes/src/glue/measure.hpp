// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <format>
#include <optional>
#include <shapes/glue/glue.hpp>
#include <string>
#include "finite.hpp"
#include "length.hpp"

namespace shapes::glue {
	// Python name and units of a derived measure, like area or volume
	struct measure_names {
		char const* name;
		char const* doc;
		char const* base_unit;  // for the docstring of `value`, e.g. "square meters"
		char const* cm_arg;     // keyword argument and suffix of `to_...`, e.g. "cm2"
		char const* mm_arg;
		char const* cm_symbol;  // for __str__, e.g. "cm²"
	};

	// `Measure` stores the base unit (m², m³), `InCm` and `InMm` are the same
	// quantity in cm-based and mm-based units; immutable, like `length`
	template <typename Measure, typename InCm, typename InMm>
	py::class_<Measure> bind_measure(py::module_& m, measure_names const& names) {
		auto const to_cm = [](Measure const& self) { return InCm{self}.count(); };
		auto const to_mm = [](Measure const& self) { return InMm{self}.count(); };
		auto const cm_arg = std::string{names.cm_arg};
		auto const mm_arg = std::string{names.mm_arg};
		auto const symbol = std::string{names.cm_symbol};

		py::class_<Measure> cls(m, names.name, names.doc);
		cls.def(py::init([cm_arg, mm_arg, name = std::string{names.name}](std::optional<double> in_cm,
		                                                                  std::optional<double> in_mm) {
			        if (in_cm && in_mm) {
				        throw py::value_error(std::format("use either `{}` or `{}`, not both", cm_arg, mm_arg));
			        }
			        if (in_cm) return Measure{InCm{finite(*in_cm, std::format("{}: {}", name, cm_arg))}};
			        if (in_mm) return Measure{InMm{finite(*in_mm, std::format("{}: {}", name, mm_arg))}};
			        return Measure{};
		        }),
		        py::kw_only(), py::arg(names.cm_arg) = py::none(), py::arg(names.mm_arg) = py::none(),
		        std::format("Measure given in `{}` or in `{}`; zero, if neither is given.", cm_arg, mm_arg).c_str())
		    .def_property_readonly("value", &Measure::count, std::format("Value in {}.", names.base_unit).c_str())
		    .def(("to_" + cm_arg).c_str(), to_cm)
		    .def(("to_" + mm_arg).c_str(), to_mm)

		    .def(-py::self)
		    .def("__hash__", [](Measure const& self) { return py::hash(py::float_(self.count())); })

		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def(py::self < py::self)
		    .def(py::self <= py::self)
		    .def(py::self > py::self)
		    .def(py::self >= py::self)

		    .def("__abs__", [](Measure const& self) { return abs(self); })
		    .def("__str__", [to_cm, symbol](
		                        Measure const& self) { return std::format("{} {}", number_repr(to_cm(self)), symbol); })
		    .def("__repr__", [to_cm, name = std::string{names.name}, cm_arg](Measure const& self) {
			    return std::format("{}({}={})", name, cm_arg, number_repr(to_cm(self)));
		    });
		def_checked_sums<Measure>(cls, names.name);
		def_checked_scaling<Measure>(cls, names.name);
		return cls;
	}
}  // namespace shapes::glue
