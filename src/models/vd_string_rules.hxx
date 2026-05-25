#pragma once

#ifndef VD_STRING_RULES_HXX
#define VD_STRING_RULES_HXX

#include <concepts>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>

#include "assert/vd_assert.hxx"

#include "inline_deps/ctre.hpp"

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

    constexpr string_match() = default;
    constexpr string_match(mode mode) : match_mode(mode) {};

    constexpr bool operator()(std::string_view s) const
    {
        bool match = Matcher(s);
        return match_mode == mode::include ? match : !match;
    }
};

// Separate type for runtime-pattern regex: cannot use string_match<> because the
// pattern is not a compile-time NTTP. Compatible with value_checker via operator().
struct regex_checker {
    enum class mode { include, exclude };

    std::string pattern;
    mode match_mode = mode::include;

    bool operator()(std::string_view s) const;
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

inline bool std_regex(std::string_view s, std::string_view pattern)
{
    try {
        std::regex re(pattern.begin(), pattern.end());
        return std::regex_match(std::string(s), re);
    }
    catch(const std::regex_error&) {
        vd::require(false, "Invalid regex pattern: {}", pattern);
        return false;
    }
}

constexpr bool uri_like(std::string_view s)
{
    constexpr auto pattern = ctll::fixed_string { R"(^\w+://\S+$)" };
    return ctre::match<pattern>(s);
}
} // namespace vd::string_rules::detail

namespace vd::string_rules
{
inline bool regex_checker::operator()(std::string_view s) const
{
    bool matched = detail::std_regex(s, pattern);
    return match_mode == mode::include ? matched : !matched;
}

constexpr string_match<detail::empty_string> empty()
{
    return { string_match<detail::empty_string>::mode::include };
}

constexpr string_match<detail::non_empty_string> non_empty()
{
    return { string_match<detail::non_empty_string>::mode::include };
}

constexpr string_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { string_match<detail::empty_or_whitespace_string>::mode::include };
}

constexpr string_match<detail::email_like> email_like()
{
    return { string_match<detail::email_like>::mode::include };
}

inline regex_checker regex(std::string pattern)
{
    return { std::move(pattern), regex_checker::mode::include };
}

constexpr string_match<detail::uri_like> uri_like()
{
    return { string_match<detail::uri_like>::mode::include };
}
} // namespace vd::string_rules

#endif // VD_STRING_RULES_HXX
