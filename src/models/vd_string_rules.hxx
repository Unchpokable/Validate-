#pragma once

#include "assert/vd_assert.hxx"
#include "core/vd_exception.hxx"
#ifndef VD_STRING_RULES_HXX
#define VD_STRING_RULES_HXX

#include <algorithm>
#include <concepts>
#include <regex>
#include <string>
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
        return evaluate(Matcher(s));
    }

    // Generic basic_string_view for any CharT other than char (wchar_t, char8_t, char16_t, char32_t).
    // CharT == char is excluded so it never competes with the non-template overload above.
    template<class CharT, class Traits>
    requires(!std::same_as<CharT, char>) && std::invocable<decltype(Matcher), std::basic_string_view<CharT, Traits>>
            && std::convertible_to<std::invoke_result_t<decltype(Matcher), std::basic_string_view<CharT, Traits>>, bool>
    vd::result operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return evaluate(Matcher(s));
    }

    // std::basic_string<CharT> for CharT other than char (e.g. std::wstring), forwarded as a view.
    // Deduction guides can't convert basic_string -> basic_string_view, so this overload exists
    // to cover that case explicitly (the char/std::string case is handled by the overload above).
    template<class CharT, class Traits, class Alloc>
    requires(!std::same_as<CharT, char>)
    vd::result operator()(const std::basic_string<CharT, Traits, Alloc>& s) const
    {
        return (*this)(std::basic_string_view<CharT, Traits>(s));
    }

private:
    vd::result evaluate(bool match) const
    {
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
struct empty_string_t {
    template<class CharT, class Traits>
    constexpr bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return s.empty();
    }
};
inline constexpr empty_string_t empty_string {};

struct non_empty_string_t {
    template<class CharT, class Traits>
    constexpr bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return !s.empty();
    }
};
inline constexpr non_empty_string_t non_empty_string {};

constexpr bool email_like(std::string_view s)
{
    constexpr auto pattern = ctll::fixed_string { R"(^\S+@\S+\.\S+$)" };
    return ctre::match<pattern>(s);
}

struct empty_or_whitespace_string_t {
    template<class CharT, class Traits>
    constexpr bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return std::all_of(s.begin(), s.end(), [](CharT c) {
            return c == CharT(' ') || c == CharT('\t') || c == CharT('\n') || c == CharT('\r') || c == CharT('\f') || c == CharT('\v');
        });
    }
};
inline constexpr empty_or_whitespace_string_t empty_or_whitespace_string {};

// Filters a strings with size shorter than given max length. WARN: this counts code units, not grapheme clusters or user-perceived
// characters.
struct max_length_t final {
    std::size_t max_len;

    constexpr max_length_t(std::size_t max_len) : max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
    }

    template<class CharT, class Traits>
    bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return s.size() <= max_len;
    }
};

// Filters a strings with size longer than given min length. WARN: this counts code units, not grapheme clusters or user-perceived
// characters.
struct min_length_t final {
    std::size_t min_len;

    constexpr min_length_t(std::size_t min_len) : min_len(min_len)
    {
        vd::ct_require<vd::assertion_exception>(min_len > 0, "min_len must be positive");
    }

    template<class CharT, class Traits>
    bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return s.size() >= min_len;
    }
};

// Filters a strings with size in between given min and max length. WARN: this counts code units, not grapheme clusters or user-perceived
// characters.
struct length_in_between_t final {
    std::size_t min_len;
    std::size_t max_len;

    constexpr length_in_between_t(std::size_t min_len, std::size_t max_len) : min_len(min_len), max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(min_len > 0, "min_len must be positive");
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
        vd::ct_require<vd::assertion_exception>(max_len >= min_len, "max_len must be greater than or equal to min_len");
    }

    template<class CharT, class Traits>
    bool operator()(std::basic_string_view<CharT, Traits> s) const
    {
        return s.size() >= min_len && s.size() <= max_len;
    }
};

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

namespace vd::string_rules
{
constexpr detail::min_length_t min_length(std::size_t min_len)
{
    return detail::min_length_t(min_len);
}

constexpr detail::max_length_t max_length(std::size_t max_len)
{
    return detail::max_length_t(max_len);
}

constexpr detail::length_in_between_t length_in_between(std::size_t min_len, std::size_t max_len)
{
    return detail::length_in_between_t(min_len, max_len);
}
} // namespace vd::string_rules

#endif // VD_STRING_RULES_HXX
