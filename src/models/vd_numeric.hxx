#pragma once

#ifndef VD_NUMERIC_HXX
#define VD_NUMERIC_HXX

#include <format>
#include <limits>
#include <type_traits>

#include "utils/vd_ctnextafter.hxx"

#include "models/vd_basic_model.hxx"

namespace vd
{
// vd::generic_numer is provided by vd_ctnextafter.hxx

template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept numeric_compatible = generic_numer<T> && arithmetic<T>;

template<numeric_compatible T>
struct numeric_bounds final {
    const T min;
    const T max;

private:
    bool inverse_condition { false };

public:
    constexpr numeric_bounds(T min, T max) : min(min), max(max)
    {
    }

    vd::result operator()(const T& value) const
    {
        bool in_range = value >= min && value <= max;
        bool passes = inverse_condition ? !in_range : in_range;

        if(passes) {
            return vd::result::ok();
        }

        if(!inverse_condition) {
            return vd::result::failed({ std::format("value {} is not in range [{}, {}]", value, min, max) });
        }
        else {
            return vd::result::failed({ std::format("value {} falls within excluded range [{}, {}]", value, min, max) });
        }
    }

    /// [min, max]
    static constexpr numeric_bounds<T> inclusive(T min, T max)
    {
        return { min, max };
    }

    /// (min, max)
    static constexpr numeric_bounds<T> exclusive(T min, T max)
    {
        return { vd::ct_nextafter(min, std::numeric_limits<T>::max()), vd::ct_nextafter(max, std::numeric_limits<T>::lowest()) };
    }

    /// (min, +inf)
    static constexpr numeric_bounds<T> greater_than(T min)
    {
        return { vd::ct_nextafter(min, std::numeric_limits<T>::max()), std::numeric_limits<T>::max() };
    }

    /// (-inf, max)
    static constexpr numeric_bounds<T> less_than(T max)
    {
        return { std::numeric_limits<T>::lowest(), vd::ct_nextafter(max, std::numeric_limits<T>::lowest()) };
    }

    /// (-inf, +inf)
    static constexpr numeric_bounds<T> unbounded()
    {
        return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max() };
    }

    /// { (-inf, lower_bound], [upper_bound, +inf) }
    static constexpr numeric_bounds<T> outside_inclusive(T lower_bound, T upper_bound)
    {
        auto bounds = exclusive(lower_bound, upper_bound);
        bounds.inverse_condition = true;
        return bounds;
    }

    /// { (-inf, lower_bound), (upper_bound, +inf) }
    static constexpr numeric_bounds<T> outside_exclusive(T lower_bound, T upper_bound)
    {
        auto bounds = inclusive(lower_bound, upper_bound);
        bounds.inverse_condition = true;
        return bounds;
    }
};

using byte_bounds = numeric_bounds<std::uint8_t>;
using short_bounds = numeric_bounds<std::int16_t>;
using int_bounds = numeric_bounds<std::int32_t>;
using long_bounds = numeric_bounds<std::int64_t>;
using float_bounds = numeric_bounds<float>;
using double_bounds = numeric_bounds<double>;

using signed_byte_bounds = numeric_bounds<std::int8_t>;
using unsigned_short_bounds = numeric_bounds<std::uint16_t>;
using unsigned_int_bounds = numeric_bounds<std::uint32_t>;
using unsigned_long_bounds = numeric_bounds<std::uint64_t>;

using byte_model = basic_model<std::uint8_t>;
using short_model = basic_model<std::int16_t>;
using int_model = basic_model<std::int32_t>;
using long_model = basic_model<std::int64_t>;
using float_model = basic_model<float>;
using double_model = basic_model<double>;

using signed_byte_model = basic_model<std::int8_t>;
using unsigned_short_model = basic_model<std::uint16_t>;
using unsigned_int_model = basic_model<std::uint32_t>;
using unsigned_long_model = basic_model<std::uint64_t>;
} // namespace vd

namespace vd::numeric
{
template<numeric_compatible T>
vd::rule<T> finite()
{
    return vd::rule<T>([](const T& value) -> vd::result {
        auto is_finite = std::isfinite(value);

        if(is_finite) {
            return vd::result::ok();
        }

        return vd::result::failed({ "Value if not finite: {}", value });
    });
}
} // namespace vd::numeric

#endif // VD_NUMERIC_HXX
