#pragma once

#ifndef VD_QSTRING_HXX
#define VD_QSTRING_HXX

#include <concepts>
#include <string_view>
#include <type_traits>

#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include "vd_core.hxx"

namespace vd::qt::string_rules
{
// Concept: a callable that accepts QStringView and returns bool-convertible.
template<auto Matcher>
concept qstring_matcher =
    (std::invocable<decltype(Matcher), QStringView> && std::convertible_to<std::invoke_result_t<decltype(Matcher), QStringView>, bool>);

// Mirrors vd::string_rules::string_match but operates on QStringView.
// Since const QString& implicitly converts to QStringView, this is also
// invocable with QString — making it compatible with vd::member on QString fields.
template<auto Matcher>
requires qstring_matcher<Matcher>
struct qstring_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;
    std::string_view check_description = "string check failed";

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m) : match_mode(m) {}
    constexpr qstring_match(mode m, std::string_view desc) : match_mode(m), check_description(desc) {}

    vd::result operator()(QStringView s) const
    {
        bool match = Matcher(s);
        bool passes = (match_mode == mode::include) ? match : !match;
        if(passes)
            return vd::result::ok();
        return vd::result::failed({ std::string(check_description) });
    }
};

// Runtime regex checker using QRegularExpression (PCRE2, full Unicode support).
// Uses full-string match semantics to mirror std::regex_match behaviour.
struct qregex_checker {
    enum class mode { include, exclude };
    QString pattern;
    mode match_mode = mode::include;

    vd::result operator()(QStringView s) const;
};
} // namespace vd::qt::string_rules

namespace vd::qt::string_rules::detail
{
bool empty_string(QStringView s);
bool non_empty_string(QStringView s);
// Uses QChar::isSpace() which covers all Unicode whitespace, matching Qt idioms.
bool empty_or_whitespace_string(QStringView s);
// Reuses the same CTRE patterns as the std counterpart via UTF-8 conversion.
bool email_like(QStringView s);
bool uri_like(QStringView s);
// Full-string match: capturedStart==0 and capturedLength==subject length,
// mirroring std::regex_match rather than std::regex_search.
bool qregex(QStringView s, const QString& pattern);
} // namespace vd::qt::string_rules::detail

namespace vd::qt::string_rules
{
inline qstring_match<detail::empty_string> empty()
{
    return { qstring_match<detail::empty_string>::mode::include, "string must be empty" };
}

inline qstring_match<detail::non_empty_string> non_empty()
{
    return { qstring_match<detail::non_empty_string>::mode::include, "string must be non-empty" };
}

inline qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { qstring_match<detail::empty_or_whitespace_string>::mode::include, "string must be empty or whitespace-only" };
}

inline qstring_match<detail::email_like> email_like()
{
    return { qstring_match<detail::email_like>::mode::include, "string does not look like an email address" };
}

inline qstring_match<detail::uri_like> uri_like()
{
    return { qstring_match<detail::uri_like>::mode::include, "string does not look like a URI" };
}

qregex_checker regex(QString pattern);
} // namespace vd::qt::string_rules

#endif // VD_QSTRING_HXX
