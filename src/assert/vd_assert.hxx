#pragma once

#ifndef VD_IMPL_ASSERT_HXX
#define VD_IMPL_ASSERT_HXX

#include <format>
#include <source_location>
#include <string_view>
#include <type_traits>

#include "core/vd_exception.hxx"

namespace vd::detail
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
[[nodiscard]] std::string format_fail(std::string_view message, const std::source_location& loc);
} // namespace vd::detail

namespace vd::detail
{
// Checked on a const lvalue on purpose: the require family takes its condition by
// const&, so a type whose operator bool() / operator!() is non-const must be rejected
// by the constraint rather than blow up inside the template body.
template<typename T>
concept vd_contextually_bool_impl = requires(const T& t) { static_cast<bool>(t); };

template<typename T>
concept contextually_bool = vd_contextually_bool_impl<T> && requires(const T& t) {
    { !t } -> vd_contextually_bool_impl;
};
} // namespace vd::detail

namespace vd
{
template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
constexpr void ct_require(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw ExceptionType(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}
} // namespace vd

namespace vd
{
template<detail::contextually_bool Cond, typename... Args>
void strict_require(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        detail::assert_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc);
    }
}

template<detail::contextually_bool Cond, typename... Args>
void require(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw vd::assertion_exception(vd::detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void require(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw ExceptionType(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
requires std::invocable<decltype(OnFailed), std::string_view>
void require_cb(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        OnFailed(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

// Debug-only family: active only when the build declares itself a debug build,
// i.e. when none of the release markers is defined. A disjunction here would keep
// these compiled in for every ordinary release build (MSVC defines NDEBUG alone).
#if not defined(_NDEBUG) and not defined(NDEBUG) and not defined(RELEASE)

template<detail::contextually_bool Cond, typename... Args>
void strict_required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        detail::assert_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc);
    }
}

template<detail::contextually_bool Cond, typename... Args>
void required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw vd::assertion_exception(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        throw ExceptionType(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
requires std::invocable<decltype(OnFailed), std::string_view>
void require_cbd(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
    if(!condition) {
        OnFailed(detail::format_fail(std::format(fmt_loc.fmt, std::forward<Args>(args)...), fmt_loc.loc));
    }
}

#else

template<detail::contextually_bool Cond, typename... Args>
void strict_required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

template<detail::contextually_bool Cond, typename... Args>
void required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
requires std::derived_from<ExceptionType, std::exception>
void required(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
requires std::invocable<decltype(OnFailed), std::string_view>
void require_cbd(const Cond& condition, detail::assert_format<std::type_identity_t<Args>...> fmt_loc, Args&&... args)
{
}

#endif

} // namespace vd

#endif // VD_IMPL_ASSERT_HXX
