#pragma once

#ifndef VD_CT_NEXT_AFTER_HXX
#define VD_CT_NEXT_AFTER_HXX

#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>

static_assert(std::numeric_limits<float>::is_iec559, "ct_nextafter: requires IEEE 754 binary32 float");
static_assert(std::numeric_limits<double>::is_iec559, "ct_nextafter: requires IEEE 754 binary64 double");

namespace vd
{

namespace detail
{

// Transforms IEEE 754 bit patterns into a monotone unsigned ordering where
// a higher integer value corresponds to a numerically greater float:
//   Positive floats (sign bit 0): set the sign bit to place them above all negatives.
//   Negative floats (sign bit 1): flip all bits so -inf maps to 0 and -0 sits just below +0.
template<typename UInt>
constexpr UInt fp_to_ordered(UInt bits) noexcept
{
    constexpr UInt sign_bit = UInt(1) << (sizeof(UInt) * 8u - 1u);
    return (bits & sign_bit) ? ~bits : (bits | sign_bit);
}

template<typename UInt>
constexpr UInt fp_from_ordered(UInt ordered) noexcept
{
    constexpr UInt sign_bit = UInt(1) << (sizeof(UInt) * 8u - 1u);
    return (ordered & sign_bit) ? (ordered ^ sign_bit) : ~ordered;
}

template<std::integral T>
constexpr T ct_nextafter_int(T from, T to) noexcept
{
    if(from == to) {
        return to;
    }

    if(from < to) {
        if(from == std::numeric_limits<T>::max()) {
            return from; // would overflow, clamp
        }
        return static_cast<T>(from + T(1));
    }

    if(from == std::numeric_limits<T>::min()) {
        return from; // would overflow, clamp
    }

    return static_cast<T>(from - T(1));
}

template<std::floating_point T>
constexpr T ct_nextafter_fp(T from, T to) noexcept
{
    // Covers the +0 == -0 case and the trivial from == to case.
    // Also covers NaN == NaN? No — NaN != NaN, so NaN is NOT caught here.
    if(from == to) {
        return to;
    }

    // NaN propagation: identity on from, and if to is NaN but from is not
    if(from != from) {
        return from;
    }

    if(to != to) {
        return to;
    }

    const bool going_up = to > from;

    // ±0 must be handled before bit manipulation: +0 and -0 compare equal,
    // but their bit patterns are on opposite sides of the signed-zero boundary.
    // Naïve bit stepping would cross +0 → -0 (or vice versa) instead of entering
    // the subnormal range, producing the wrong neighbour.
    if(from == T(0)) {
        return going_up ? std::numeric_limits<T>::denorm_min() : -std::numeric_limits<T>::denorm_min();
    }

    if constexpr(sizeof(T) == 4) {
        auto bits = std::bit_cast<uint32_t>(from);
        auto ord = fp_to_ordered(bits);
        ord = going_up ? ord + 1u : ord - 1u;
        return std::bit_cast<T>(fp_from_ordered(ord));
    }
    else if constexpr(sizeof(T) == 8) {
        auto bits = std::bit_cast<uint64_t>(from);
        auto ord = fp_to_ordered(bits);
        ord = going_up ? ord + 1ull : ord - 1ull;
        return std::bit_cast<T>(fp_from_ordered(ord));
    }
    else {
        // sizeof(T) == 0 is always false but is type-dependent, so this static_assert
        // only fires when this else branch is actually instantiated (unsupported T).
        static_assert(sizeof(T) == 0,
            "ct_nextafter: unsupported floating-point type size; "
            "supported sizes are 4 bytes (float) and 8 bytes (double, "
            "long double on MSVC / ARM). "
            "On GCC/x86, __float128 is supported when __SIZEOF_INT128__ is defined; "
            "80-bit long double in a 16-byte container is not supported.");
        return from; // unreachable — satisfies the return-type requirement
    }
}

} // namespace detail

template<typename T>
concept numeric = std::integral<T> || std::floating_point<T>;

template<numeric T>
[[nodiscard]] constexpr T ct_nextafter(T from, T to) noexcept
{
    if constexpr(std::integral<T>)
        return detail::ct_nextafter_int(from, to);
    else
        return detail::ct_nextafter_fp(from, to);
}

} // namespace vd

#endif
