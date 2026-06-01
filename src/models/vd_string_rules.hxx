#pragma once

#ifndef VD_STRING_RULES_HXX
#define VD_STRING_RULES_HXX

#include <concepts>
#include <regex>
#include <string_view>
#include <type_traits>

#include "inline_deps/ctre.hpp"

#include "core/vd_result.hxx"

namespace vd::string_rules
{
template<auto Matcher>
concept string_matcher = (std::invocable<decltype(Matcher), std::string_view>
                          && std::convertible_to<std::invoke_result_t<decltype(Matcher), std::string_view>, bool>);

template<auto Matcher>
requires string_matcher<Matcher>
struct string_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;
    std::string check_description = "string check failed";

    constexpr string_match() = default;
    constexpr string_match(mode m) : match_mode(m)
    {
    }
    constexpr string_match(mode m, std::string_view desc) : match_mode(m), check_description(desc)
    {
    }

    vd::result operator()(std::string_view s) const
    {
        bool match = Matcher(s);
        bool passes = (match_mode == mode::include) ? match : !match;
        if(passes)
            return vd::result::ok();
        return vd::result::failed({ std::string(check_description) });
    }
};

// Separate type for runtime-pattern regex: cannot use string_match<> because the
// pattern is not a compile-time NTTP. Compatible with value_checker via operator().
struct regex_checker {
    enum class mode { include, exclude };

    std::regex pattern;
    mode match_mode = mode::include;

    vd::result operator()(std::string_view s) const;
};
} // namespace vd::string_rules

namespace vd::string_rules::detail
{
constexpr bool empty_string(std::string_view s)
{
    return s.empty();
}

constexpr bool non_empty_string(std::string_view s)
{
    return !s.empty();
}

constexpr bool email_like(std::string_view s)
{
    constexpr auto pattern = ctll::fixed_string { R"(^\S+@\S+\.\S+$)" };
    return ctre::match<pattern>(s);
}

constexpr bool empty_or_whitespace_string(std::string_view s)
{
    return s.find_first_not_of(" \t\n\r\f\v") == std::string_view::npos;
}

constexpr bool uri_like(std::string_view s)
{
    constexpr auto pattern = ctll::fixed_string { R"(^\w+://\S+$)" };
    return ctre::match<pattern>(s);
}
} // namespace vd::string_rules::detail

namespace vd::string_rules
{
constexpr string_match<detail::empty_string> empty()
{
    return { string_match<detail::empty_string>::mode::include, "string must be empty" };
}

constexpr string_match<detail::non_empty_string> non_empty()
{
    return { string_match<detail::non_empty_string>::mode::include, "string must be non-empty" };
}

constexpr string_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { string_match<detail::empty_or_whitespace_string>::mode::include, "string must be empty or whitespace-only" };
}

constexpr string_match<detail::email_like> email_like()
{
    return { string_match<detail::email_like>::mode::include, "string does not look like an email address" };
}

regex_checker regex(std::string_view pattern);

constexpr string_match<detail::uri_like> uri_like()
{
    return { string_match<detail::uri_like>::mode::include, "string does not look like a URI" };
}
} // namespace vd::string_rules

#endif // VD_STRING_RULES_HXX
