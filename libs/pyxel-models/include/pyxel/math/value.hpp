// Copyright (c) 2025 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#pragma once

#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
#include <numbers>
#include <numeric>
#include <ratio>
#include <type_traits>

namespace pyxel::math {
	template <typename S>
	concept Scalar = std::integral<S> || std::floating_point<S>;

	template <typename Tag, typename Storage, typename Ratio = std::ratio<1>>
	struct value;

	template <typename T>
	inline constexpr bool is_value_v = false;
	template <typename Tag, typename Storage, typename Ratio>
	inline constexpr bool is_value_v<value<Tag, Storage, Ratio>> = true;

	template <typename T>
	concept quantity = is_value_v<std::remove_cv_t<T>>;

	// A unit of `Ratio * π`; used for units, which are rational fractions of a
	// half-turn, such as degrees, and are irrational in radians.
	template <typename Ratio>
	struct pi_scaled {
		using ratio = Ratio;
	};

	namespace detail {
		template <typename Ratio>
		struct unit_traits {
			using ratio = Ratio;
			static constexpr int pi_power = 0;
		};

		template <typename Ratio>
		struct unit_traits<pi_scaled<Ratio>> {
			using ratio = Ratio;
			static constexpr int pi_power = 1;
		};

		template <typename Ratio>
		using base_ratio_t = typename unit_traits<Ratio>::ratio;

		template <typename Ratio>
		inline constexpr int pi_power_v = unit_traits<Ratio>::pi_power;

		// A conversion is lossless, if the target can represent every value of
		// the source: either the target is floating-point, or both are integral
		// and the source unit is a whole multiple of the target unit.
		template <typename StorageIn, typename RatioIn, typename StorageOut, typename RatioOut>
		inline constexpr bool is_lossless_v =
		    std::floating_point<StorageOut> ||
		    (!std::floating_point<StorageIn> && pi_power_v<RatioIn> == pi_power_v<RatioOut> &&
		     std::ratio_divide<base_ratio_t<RatioIn>, base_ratio_t<RatioOut>>::den == 1);

		// UNSAFE RANGES: nothing here checks the range; for now, the callers
		// must keep the values within it.
		//
		// - floating-point to integral storage: NaN, infinity, and any value
		//   past the range of the target (about 9.2e12 m for micrometers,
		//   9.2e9 m for nanometers) are undefined behaviour of static_cast;
		//   in practice, x86 returns LLONG_MIN, other platforms may not.
		// - integral to integral, towards a finer unit (metres to
		//   micrometers): the multiplication overflows signed integers,
		//   which is undefined behaviour as well.
		// - floating-point to floating-point is well-defined: NaN and
		//   infinity pass through, and a finite value multiplied past the
		//   range of a double becomes infinity.
		//
		// Reached through the converting constructor, value_cast and round
		// below. See "Range-checked unit conversions" in docs/roadmap.md.
		template <typename StorageOut, typename RatioOut, typename StorageIn, typename RatioIn>
		constexpr StorageOut convert(StorageIn count) noexcept {
			using div = std::ratio_divide<base_ratio_t<RatioIn>, base_ratio_t<RatioOut>>;
			using common = std::common_type_t<StorageIn, StorageOut, std::intmax_t>;
			constexpr int pi_diff = pi_power_v<RatioIn> - pi_power_v<RatioOut>;

			if constexpr (pi_diff != 0) {
				static_assert(std::floating_point<StorageOut>, "units scaled by pi need floating-point storage");
				auto result = static_cast<StorageOut>(count) * static_cast<StorageOut>(div::num);
				if constexpr (pi_diff > 0)
					result *= std::numbers::pi_v<StorageOut>;
				else
					result /= std::numbers::pi_v<StorageOut>;
				return result / static_cast<StorageOut>(div::den);
			} else if constexpr (div::num == 1 && div::den == 1) {
				return static_cast<StorageOut>(count);
			} else if constexpr (div::den == 1) {
				return static_cast<StorageOut>(static_cast<common>(count) * static_cast<common>(div::num));
			} else if constexpr (div::num == 1) {
				return static_cast<StorageOut>(static_cast<common>(count) / static_cast<common>(div::den));
			} else {
				return static_cast<StorageOut>(static_cast<common>(count) * static_cast<common>(div::num) /
				                               static_cast<common>(div::den));
			}
		}

		// the finest unit both sides convert to exactly; if only one of them is
		// scaled by pi, there is none and the base unit is used
		template <typename RatioOne, typename RatioOther>
		consteval auto common_ratio() noexcept {
			using one = base_ratio_t<RatioOne>;
			using other = base_ratio_t<RatioOther>;
			using base = std::ratio<std::gcd(one::num, other::num), std::lcm(one::den, other::den)>;

			if constexpr (pi_power_v<RatioOne> != pi_power_v<RatioOther>)
				return std::type_identity<std::ratio<1>>{};
			else if constexpr (pi_power_v<RatioOne> == 0)
				return std::type_identity<base>{};
			else
				return std::type_identity<pi_scaled<base>>{};
		}

		template <typename Tag, typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
		using common_value_t = value<Tag,
		                             std::common_type_t<StorageOne, StorageOther>,
		                             typename decltype(common_ratio<RatioOne, RatioOther>())::type>;
	}  // namespace detail

	template <typename Tag, typename Storage, typename Ratio>
	struct value {
		using tag = Tag;
		using storage = Storage;
		using ratio = Ratio;

		Storage v{};

		constexpr value() noexcept = default;

		// integral counts only accept integral numbers
		template <Scalar S>
		    requires(std::floating_point<Storage> || std::integral<S>)
		explicit constexpr value(S count) noexcept : v{static_cast<Storage>(count)} {}

		// implicit only, when no precision can be lost; explicit conversions
		// to integral storage are not range-checked, see detail::convert
		template <typename StorageIn, typename RatioIn>
		    requires(!std::is_same_v<value, value<Tag, StorageIn, RatioIn>>)
		constexpr explicit(!detail::is_lossless_v<StorageIn, RatioIn, Storage, Ratio>)
		    value(value<Tag, StorageIn, RatioIn> const& other) noexcept
		    : v{detail::convert<Storage, Ratio, StorageIn, RatioIn>(other.count())} {}

		constexpr Storage count() const noexcept { return v; }
		constexpr Storage const& get() const noexcept { return v; }
		constexpr value abs() const noexcept { return value{v < 0 ? -v : v}; }

		constexpr value& operator+=(value const& rhs) noexcept {
			v += rhs.count();
			return *this;
		}

		constexpr value& operator-=(value const& rhs) noexcept {
			v -= rhs.count();
			return *this;
		}

		template <Scalar S>
		    requires(std::floating_point<Storage> || std::integral<S>)
		constexpr value& operator*=(S rhs) noexcept {
			v = static_cast<Storage>(v * rhs);
			return *this;
		}

		template <Scalar S>
		    requires(std::floating_point<Storage> || std::integral<S>)
		constexpr value& operator/=(S rhs) noexcept {
			v = static_cast<Storage>(v / rhs);
			return *this;
		}
	};

	// ---- casts -----------------------------------------------------------

	// Explicit conversion, truncating towards zero. Not range-checked: NaN,
	// infinity or a value past an integral target is UB, see detail::convert.
	template <quantity To, typename Storage, typename Ratio>
	constexpr To value_cast(value<typename To::tag, Storage, Ratio> const& in) noexcept {
		return To{detail::convert<typename To::storage, typename To::ratio, Storage, Ratio>(in.count())};
	}

	// Explicit conversion, rounding half away from zero. Not range-checked:
	// NaN, infinity or a value past an integral target is UB, see
	// detail::convert; format_dyn in format.hpp checks before calling it.
	template <quantity To, typename Storage, typename Ratio>
	constexpr To round(value<typename To::tag, Storage, Ratio> const& in) noexcept {
		using out = typename To::storage;
		auto const exact = detail::convert<double, typename To::ratio, Storage, Ratio>(in.count());
		if constexpr (std::floating_point<out>) {
			return To{static_cast<out>(exact)};
		} else {
			return To{static_cast<out>(exact < 0 ? exact - 0.5 : exact + 0.5)};
		}
	}

	// ---- comparison ------------------------------------------------------

	template <typename Tag, typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr bool operator==(value<Tag, StorageOne, RatioOne> const& lhs,
	                          value<Tag, StorageOther, RatioOther> const& rhs) noexcept {
		using common = detail::common_value_t<Tag, StorageOne, RatioOne, StorageOther, RatioOther>;
		return common{lhs}.count() == common{rhs}.count();
	}

	template <typename Tag, typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator<=>(value<Tag, StorageOne, RatioOne> const& lhs,
	                           value<Tag, StorageOther, RatioOther> const& rhs) noexcept {
		using common = detail::common_value_t<Tag, StorageOne, RatioOne, StorageOther, RatioOther>;
		return common{lhs}.count() <=> common{rhs}.count();
	}

	// ---- arithmetic ------------------------------------------------------

	template <typename Tag, typename Storage, typename Ratio>
	constexpr value<Tag, Storage, Ratio> operator-(value<Tag, Storage, Ratio> const& in) noexcept {
		return value<Tag, Storage, Ratio>{-in.count()};
	}

	template <typename Tag, typename Storage, typename Ratio>
	constexpr value<Tag, Storage, Ratio> abs(value<Tag, Storage, Ratio> const& in) noexcept {
		return in.abs();
	}

	// mixed units are added in the finer unit, mixed storage in the wider type
	template <typename Tag, typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator+(value<Tag, StorageOne, RatioOne> const& lhs,
	                         value<Tag, StorageOther, RatioOther> const& rhs) noexcept {
		using common = detail::common_value_t<Tag, StorageOne, RatioOne, StorageOther, RatioOther>;
		return common{common{lhs}.count() + common{rhs}.count()};
	}

	template <typename Tag, typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator-(value<Tag, StorageOne, RatioOne> const& lhs,
	                         value<Tag, StorageOther, RatioOther> const& rhs) noexcept {
		using common = detail::common_value_t<Tag, StorageOne, RatioOne, StorageOther, RatioOther>;
		return common{common{lhs}.count() - common{rhs}.count()};
	}

	// scaling by a floating-point number gives a floating-point value
	template <typename Tag, typename Storage, typename Ratio, Scalar S>
	constexpr auto operator*(value<Tag, Storage, Ratio> const& lhs, S rhs) noexcept {
		using common = std::common_type_t<Storage, S>;
		return value<Tag, common, Ratio>{static_cast<common>(lhs.count()) * static_cast<common>(rhs)};
	}

	template <typename Tag, typename Storage, typename Ratio, Scalar S>
	constexpr auto operator*(S lhs, value<Tag, Storage, Ratio> const& rhs) noexcept {
		return rhs * lhs;
	}

	template <typename Tag, typename Storage, typename Ratio, Scalar S>
	constexpr auto operator/(value<Tag, Storage, Ratio> const& lhs, S rhs) noexcept {
		using common = std::common_type_t<Storage, S>;
		return value<Tag, common, Ratio>{static_cast<common>(lhs.count()) / static_cast<common>(rhs)};
	}

	// ---- angles ----------------------------------------------------------

	template <typename Storage, typename Ratio = std::ratio<1>>
	using angle = value<class AngleTag, Storage, Ratio>;

	using radians = angle<double>;
	using degrees = angle<double, pi_scaled<std::ratio<1, 180>>>;

	inline double sin(radians angle) { return std::sin(angle.count()); }
	inline double cos(radians angle) { return std::cos(angle.count()); }

	// ---- lengths, areas and volumes --------------------------------------

	template <typename Storage, typename Ratio = std::ratio<1>>
	using distance = value<class DistanceTag, Storage, Ratio>;

	template <typename Storage, typename Ratio = std::ratio<1>>
	using area = value<class AreaTag, Storage, Ratio>;

	template <typename Storage, typename Ratio = std::ratio<1>>
	using volume = value<class VolumeTag, Storage, Ratio>;

	template <typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator*(distance<StorageOne, RatioOne> const& lhs,
	                         distance<StorageOther, RatioOther> const& rhs) noexcept {
		using common = std::common_type_t<StorageOne, StorageOther>;
		return area<common, std::ratio_multiply<RatioOne, RatioOther>>{static_cast<common>(lhs.count()) *
		                                                               static_cast<common>(rhs.count())};
	}

	template <typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator*(area<StorageOne, RatioOne> const& lhs,
	                         distance<StorageOther, RatioOther> const& rhs) noexcept {
		using common = std::common_type_t<StorageOne, StorageOther>;
		return volume<common, std::ratio_multiply<RatioOne, RatioOther>>{static_cast<common>(lhs.count()) *
		                                                                 static_cast<common>(rhs.count())};
	}

	template <typename StorageOne, typename RatioOne, typename StorageOther, typename RatioOther>
	constexpr auto operator*(distance<StorageOne, RatioOne> const& lhs,
	                         area<StorageOther, RatioOther> const& rhs) noexcept {
		return rhs * lhs;
	}

	using metres = distance<long long>;
	using decimeters = distance<long long, std::deci>;
	using centimeters = distance<long long, std::centi>;
	using millimeters = distance<long long, std::milli>;
	using micrometers = distance<long long, std::micro>;
	using nanometers = distance<long long, std::nano>;
	using metres_f = distance<double>;
	using decimeters_f = distance<double, std::deci>;
	using centimeters_f = distance<double, std::centi>;
	using millimeters_f = distance<double, std::milli>;
	using micrometers_f = distance<double, std::micro>;
	using nanometers_f = distance<double, std::nano>;

	// ---- literals --------------------------------------------------------

	inline namespace literals {
		constexpr radians operator""_rad(long double v) noexcept { return radians{static_cast<double>(v)}; }
		constexpr radians operator""_rad(unsigned long long v) noexcept { return radians{static_cast<double>(v)}; }
		constexpr degrees operator""_deg(long double v) noexcept { return degrees{static_cast<double>(v)}; }
		constexpr degrees operator""_deg(unsigned long long v) noexcept { return degrees{static_cast<double>(v)}; }

#define PYXEL_MATH_LENGTH_UDL(TYPE, SUFFIX)                                                                       \
	constexpr TYPE operator""_##SUFFIX(unsigned long long v) noexcept { return TYPE{static_cast<long long>(v)}; } \
	constexpr TYPE##_f operator""_##SUFFIX(long double v) noexcept { return TYPE##_f{static_cast<double>(v)}; }

		PYXEL_MATH_LENGTH_UDL(metres, m)
		PYXEL_MATH_LENGTH_UDL(decimeters, dm)
		PYXEL_MATH_LENGTH_UDL(centimeters, cm)
		PYXEL_MATH_LENGTH_UDL(millimeters, mm)
		PYXEL_MATH_LENGTH_UDL(micrometers, um)
		PYXEL_MATH_LENGTH_UDL(nanometers, nm)

#undef PYXEL_MATH_LENGTH_UDL
	}  // namespace literals
}  // namespace pyxel::math
