#pragma once

#ifndef VD_IMPL_ASSERT_HXX
#define VD_IMPL_ASSERT_HXX

#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <source_location>
#include <string_view>
#include <type_traits>

namespace vd::details
{
// Wraps a format string and captures source_location at the call site via its consteval ctor.
// Args are deduced only from the trailing args..., not from this parameter (type_identity_t).
template<typename... Args>
struct assert_format {
    std::format_string<Args...> fmt;
    std::source_location loc;

    template<typename Fmt>
    consteval assert_format(Fmt&& f, std::source_location l = std::source_location::current()) : fmt(std::forward<Fmt>(f)), loc(l)
    {
    }
};

[[noreturn]] inline void assert_fail(std::string_view message, const std::source_location& loc)
{
    std::fprintf(stderr,
        "Assertion failed: %s\nFile: %s\nLine: %d\nFunction: %s\n",
        message.data(),
        loc.file_name(),
        static_cast<int>(loc.line()),
        loc.function_name());
    std::abort();
}
} // namespace vd::details

namespace vd
{
template<typename... Args>
inline void require(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        details::assert_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc);
    }
}

template<auto OnFailed, typename... Args>
requires std::invocable<decltype(OnFailed), std::string>
inline void require_callback(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        OnFailed(std::format(fmt_loc.fmt, std::forward<Args>(args)...));
    }
}
} // namespace vd

#endif // VD_IMPL_ASSERT_HXX
