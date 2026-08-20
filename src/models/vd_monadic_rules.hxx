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
template<typename T, typename E>
constexpr auto as_expected()
{
    return [](const std::expected<T, E>& expected) -> vd::result {
        if(expected.has_value()) {
            return vd::result::ok();
        }
        else {
            return vd::result::failed({ "Value must not be empty" });
        }
    };
}
} // namespace vd::monadic

#endif

namespace vd::monadic
{
template<typename T>
constexpr auto not_empty()
{
    return [](const std::optional<T>& opt) -> vd::result {
        if(opt.has_value()) {
            return vd::result::ok();
        }
        else {
            return vd::result::failed({ "Value must not be empty" });
        }
    };
}
} // namespace vd::monadic

#endif