#pragma once

#ifndef VD_IMPL_ASSERT_HXX
#define VD_IMPL_ASSERT_HXX

#include <format>
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

[[noreturn]] void assert_fail(std::string_view message, const std::source_location& loc);
} // namespace vd::details

namespace vd
{
template<typename... Args>
void require(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        details::assert_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc);
    }
}

template<typename ExceptionType, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void require(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw ExceptionType(std::format(fmt_loc.fmt, std::forward<Args>(args)...));
    }
}

template<auto OnFailed, typename... Args>
requires std::invocable<decltype(OnFailed), std::string_view>
void require_callback(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        OnFailed(std::format(fmt_loc.fmt, std::forward<Args>(args)...));
    }
}

#ifndef _NDEBUG

/// same as require() but working only in debug builds
template<typename... Args>
void required(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        details::assert_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc);
    }
}

/// same as require() but working only in debug builds
template<typename ExceptionType, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void required(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw ExceptionType(std::format(fmt_loc.fmt, std::forward<Args>(args)...));
    }
}

/// same as require_callback() but working only in debug builds
template<auto OnFailed, typename... Args>
requires std::invocable<decltype(OnFailed), std::string_view>
void require_callbackd(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        OnFailed(std::format(fmt_loc.fmt, std::forward<Args>(args)...));
    }
}

#else

template<typename... Args>
void required(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

template<typename ExceptionType, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void required(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

template<auto OnFailed, typename... Args>
requires std::invocable<decltype(OnFailed), std::string>
void require_callbackd(bool condition, details::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

#endif

} // namespace vd

#endif // VD_IMPL_ASSERT_HXX
