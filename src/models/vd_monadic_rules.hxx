#pragma once

#ifndef VD_MONADIC_RULES_HXX
#define VD_MONADIC_RULES_HXX

#include <version>

#if defined(__cpp_lib_expected)
#include <expected>
#endif

#include <optional>

#include "core/vd_result.hxx"

#if defined(__cpp_lib_expected)

namespace vd::monadic
{
struct as_expected_t final {
    template<typename T, typename E>
    vd::result operator()(const std::expected<T, E>& expected) const
    {
        if(expected.has_value()) {
            return vd::result::ok();
        }
        else {
            return vd::result::failed("Value is unexpected");
        }
    }
};

inline constexpr as_expected_t as_expected;
} // namespace vd::monadic

#endif

namespace vd::monadic
{
struct not_empty_t final {
    template<typename T>
    vd::result operator()(const std::optional<T>& opt) const
    {
        if(opt.has_value()) {
            return vd::result::ok();
        }
        else {
            return vd::result::failed("Value must not be empty!");
        }
    }
};

inline constexpr not_empty_t not_empty;
} // namespace vd::monadic

#endif
