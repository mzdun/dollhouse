// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pybind11/native_enum.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <cmath>
#include <format>
#include <optional>
#include <pyxel/gfx/light.hpp>
#include <pyxel/math/format.hpp>
#include <shapes/glue/glue.hpp>
#include <string>
#include "length.hpp"

namespace shapes::glue {
	using pyxel::gfx::angle;
	using pyxel::gfx::color;
	using pyxel::gfx::length;
	using pyxel::gfx::light;
	using pyxel::gfx::light_type;
	using pyxel::gfx::offset;
	using cone = light::cone_t;
	using light_area = light::area_t;
	using pyxel::gfx::area_shape;

	namespace {
		std::string angle_repr(angle value) {
			return std::format("deg({})", number_repr(pyxel::math::degrees{value}.count()));
		}

		std::string color_repr(color const& self) {
			// uint8_t would be formatted as a character
			int const r = self.r, g = self.g, b = self.b;
			if (self.alpha == 1.0f) return std::format("rgb({}, {}, {})", r, g, b);
			return std::format("rgb({}, {}, {}, {})", r, g, b, number_repr(self.alpha));
		}

		std::string cone_repr(cone const& self) {
			return std::format("light_cone(inner_angle={}, outer_angle={})", angle_repr(self.inner_angle),
			                   angle_repr(self.outer_angle));
		}

		std::string shape_repr(area_shape shape) {
			switch (shape) {
				case area_shape::rectangle:
					return "area_shape.rectangle";
				case area_shape::disk:
					return "area_shape.disk";
				case area_shape::ellipse:
					return "area_shape.ellipse";
				case area_shape::square:
					break;
			}
			return "area_shape.square";
		}

		std::string area_repr(light_area const& self) {
			return std::format("light_area(shape={}, width={}, height={}, spread={})", shape_repr(self.shape),
			                   length_repr(self.width), length_repr(self.height), angle_repr(self.spread));
		}

		std::string type_repr(light_type type) {
			switch (type) {
				case light_type::directional:
					return "light_type.directional";
				case light_type::spot:
					return "light_type.spot";
				case light_type::area:
					return "light_type.area";
				case light_type::point:
					break;
			}
			return "light_type.point";
		}

		std::string light_repr(light const& self) {
			return std::format(
			    "light(type={}, name={}, color={}, intensity={}, range={}, cone={}, area={})", type_repr(self.type),
			    py::repr(py::str(self.name)).cast<std::string>(), color_repr(self.color), number_repr(self.intensity),
			    self.range ? length_repr(*self.range) : "None", cone_repr(self.cone), area_repr(self.area));
		}

		float checked_intensity(float value) {
			if (!(value >= 0.0f) || std::isinf(value)) {
				throw py::value_error(std::format("light: intensity must be finite and not negative, not {}", value));
			}
			return value;
		}

		std::optional<length> checked_range(std::optional<length> value) {
			if (value && !(value->count() > 0.0)) {
				throw py::value_error(
				    std::format("light: range must be positive or None, not {}", length_repr(*value)));
			}
			return value;
		}

	}  // namespace

	void bind_light(py::module_& m) {
		py::native_enum<light_type>(m, "light_type", "enum.Enum", "Kind of a light source.")
		    .value("point", light_type::point)
		    .value("directional", light_type::directional)
		    .value("spot", light_type::spot)
		    .value("area", light_type::area)
		    .finalize();

		py::class_<cone>(m, "light_cone",
		                 "Cone of a spot light, as in KHR_lights_punctual; angles are measured from its axis (the "
		                 "local -z) to the edge, and are expected to keep 0 <= inner < outer <= 90 degrees.")
		    .def(py::init([](angle inner, angle outer) { return cone{.inner_angle = inner, .outer_angle = outer}; }),
		         py::arg_v("inner_angle", angle{}, "deg(0)"), py::arg_v("outer_angle", pyxel::gfx::deg(45), "deg(45)"))
		    .def_readwrite("inner_angle", &cone::inner_angle, "Full light inside this angle.")
		    .def_readwrite("outer_angle", &cone::outer_angle, "No light outside this angle.")
		    .def("at", &cone::at, "off_axis"_a,
		         "Fraction of the light at `off_axis` angle from the cone's axis; squared, as in KHR_lights_punctual.")
		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def("__repr__", &cone_repr);

		py::native_enum<area_shape>(m, "area_shape", "enum.Enum", "Shape of the emitting surface of an area light.")
		    .value("square", area_shape::square)
		    .value("rectangle", area_shape::rectangle)
		    .value("disk", area_shape::disk)
		    .value("ellipse", area_shape::ellipse)
		    .finalize();

		py::class_<light_area>(m, "light_area",
		                       "Emitting surface of an area light, in the local x-y plane of the light, centered at "
		                       "its origin and facing -z; the width runs along x, the height along y.")
		    .def(py::init([](area_shape shape, length width, length height, angle spread) {
			         return light_area{.shape = shape, .width = width, .height = height, .spread = spread};
		         }),
		         py::arg_v("shape", area_shape::square, "area_shape.square"),
		         py::arg_v("width", length{pyxel::math::metres_f{1}}, "cm(100)"),
		         py::arg_v("height", length{pyxel::math::metres_f{1}}, "cm(100)"),
		         py::arg_v("spread", pyxel::gfx::deg(180), "deg(180)"))
		    .def_readwrite("shape", &light_area::shape)
		    .def_readwrite("width", &light_area::width, "Side of a square or rectangle, diameter of a disk or ellipse.")
		    .def_readwrite("height", &light_area::height, "Used by rectangles and ellipses only.")
		    .def_readwrite("spread", &light_area::spread,
		                   "Full angle of the emission, centered on the light's direction (-z); 180° covers the "
		                   "whole front half-space.")
		    .def("effective_height", &light_area::effective_height, "The height, or the width for squares and disks.")
		    .def("surface", &light_area::surface, "Area of the emitting surface.")
		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def("__repr__", &area_repr);

		py::class_<light>(
		    m, "light",
		    "Light source, as in glTF 2.0 KHR_lights_punctual: the falloff is physical (1 / d^2, windowed "
		    "to zero at `range`), and the brightness is the `intensity`. Area lights are an addition, with "
		    "no counterpart in KHR_lights_punctual. A light has no position or direction: it sits at the "
		    "origin of its local space and shines along -z; it is placed only by the matrix of its group.")
		    .def(py::init([](light_type type, std::string name, color color, float intensity,
		                     std::optional<length> range, cone cone, light_area area) {
			         return light{.type = type,
			                      .name = std::move(name),
			                      .color = color,
			                      .intensity = checked_intensity(intensity),
			                      .range = checked_range(range),
			                      .cone = cone,
			                      .area = area};
		         }),
		         py::kw_only(), py::arg_v("type", light_type::point, "light_type.point"), "name"_a = std::string{},
		         py::arg_v("color", pyxel::gfx::rgb(255, 255, 255), "rgb(255, 255, 255)"), "intensity"_a = 1.0f,
		         "range"_a = py::none(), "cone"_a = cone{}, "area"_a = light_area{})
		    .def_readwrite("type", &light::type)
		    .def_readwrite("name", &light::name)
		    .def_readwrite("color", &light::color, "sRGB; the alpha is not used.")
		    .def_property(
		        "intensity", [](light const& self) { return self.intensity; },
		        [](light& self, float value) { self.intensity = checked_intensity(value); },
		        "Candela for point, spot and area lights, lux for directional lights.")
		    .def_property(
		        "range", [](light const& self) { return self.range; },
		        [](light& self, std::optional<length> value) { self.range = checked_range(value); },
		        "Distance, at which the light reaches zero; None is infinite. Ignored by directional lights.")
		    .def_readwrite("cone", &light::cone, "Used by spot lights.")
		    .def_readwrite("area", &light::area, "Used by area lights.")
		    .def("falloff_at", &light::falloff_at, "distance"_a,
		         "1 / d^2, windowed to zero at `range`, if any; infinite at zero distance.")
		    .def("illuminance_at", &light::illuminance_at, "point"_a,
		         "Illuminance at `point`, given in the local space of the light, in lux, on a surface facing the "
		         "light: the intensity, with the falloff and the spot cone or the area spread.")
		    .def(py::self == py::self)
		    .def(py::self != py::self)
		    .def("__repr__", &light_repr);
	}
}  // namespace shapes::glue
