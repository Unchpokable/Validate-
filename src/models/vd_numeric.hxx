#pragma once

#ifndef VD_NUMERIC_HXX
#define VD_NUMERIC_HXX

#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

#include "vd_basic_model.hxx"

namespace vd
{
template<typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept numeric_compatible = numeric<T> && arithmetic<T>;

template<numeric_compatible T>
struct numeric_bounds final {
    const T min;
    const T max;

    numeric_bounds(T min, T max) : min(min), max(max)
    {
    }

    bool operator()(const T& value) const
    {
        return value >= min && value <= max;
    }

    static numeric_bounds<T> inclusive(T min, T max)
    {
        return { min, max };
    }

    static numeric_bounds<T> exclusive(T min, T max)
    {
        return { std::nextafter(min, std::numeric_limits<T>::max()), std::nextafter(max, std::numeric_limits<T>::lowest()) };
    }

    static numeric_bounds<T> greater_than(T min)
    {
        return { std::nextafter(min, std::numeric_limits<T>::max()), std::numeric_limits<T>::max() };
    }

    static numeric_bounds<T> less_than(T max)
    {
        return { std::numeric_limits<T>::lowest(), std::nextafter(max, std::numeric_limits<T>::lowest()) };
    }

    static numeric_bounds<T> unbounded()
    {
        return { std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max() };
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

#endif // VD_NUMERIC_HXX