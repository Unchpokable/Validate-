#pragma once

#ifndef VD_QSTRING_HXX
#define VD_QSTRING_HXX

#include <concepts>
#include <string_view>
#include <type_traits>

#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include "assert/vd_assert.hxx"
#include "inline_deps/ctre.hpp"

namespace vd::qt::string_rules
{
// Concept: a callable that accepts QStringView and returns bool-convertible.
template<auto Matcher>
concept qstring_matcher = (std::invocable<decltype(Matcher), QStringView>
                           && std::convertible_to<std::invoke_result_t<decltype(Matcher), QStringView>, bool>);

// Mirrors vd::string_rules::string_match but operates on QStringView.
// Since const QString& implicitly converts to QStringView, this is also
// invocable with QString — making it compatible with vd::member on QString fields.
template<auto Matcher>
requires qstring_matcher<Matcher>
struct qstring_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m) : match_mode(m) {}

    bool operator()(QStringView s) const
    {
        bool match = Matcher(s);
        return match_mode == mode::include ? match : !match;
    }
};

// Runtime regex checker using QRegularExpression (PCRE2, full Unicode support).
// Uses full-string match semantics to mirror std::regex_match behaviour.
struct qregex_checker {
    enum class mode { include, exclude };
    QString pattern;
    mode match_mode = mode::include;

    bool operator()(QStringView s) const;
};
} // namespace vd::qt::string_rules

namespace vd::qt::string_rules::detail
{
inline bool empty_string(QStringView s)
{
    return s.isEmpty();
}

inline bool non_empty_string(QStringView s)
{
    return !s.isEmpty();
}

// Uses QChar::isSpace() which covers all Unicode whitespace, matching Qt idioms.
inline bool empty_or_whitespace_string(QStringView s)
{
    if (s.isEmpty())
        return true;
    for (QChar c : s)
        if (!c.isSpace())
            return false;
    return true;
}

// Reuses the same CTRE patterns as the std counterpart via UTF-8 conversion.
inline bool email_like(QStringView s)
{
    auto utf8 = s.toUtf8();
    constexpr auto pattern = ctll::fixed_string { R"(^\S+@\S+\.\S+$)" };
    return ctre::match<pattern>(std::string_view(utf8.data(), utf8.size()));
}

inline bool uri_like(QStringView s)
{
    auto utf8 = s.toUtf8();
    constexpr auto pattern = ctll::fixed_string { R"(^\w+://\S+$)" };
    return ctre::match<pattern>(std::string_view(utf8.data(), utf8.size()));
}

// Full-string match: capturedStart==0 and capturedLength==subject length,
// mirroring std::regex_match rather than std::regex_search.
inline bool qregex(QStringView s, const QString& pattern)
{
    QRegularExpression re(pattern);
    if (!re.isValid()) {
        vd::require(false, "Invalid regex pattern: {}", pattern.toStdString());
        return false;
    }
    QRegularExpressionMatch m = re.match(s);
    return m.hasMatch() && m.capturedStart() == 0 && m.capturedLength() == s.size();
}
} // namespace vd::qt::string_rules::detail

namespace vd::qt::string_rules
{
inline bool qregex_checker::operator()(QStringView s) const
{
    bool matched = detail::qregex(s, pattern);
    return match_mode == mode::include ? matched : !matched;
}

inline qstring_match<detail::empty_string> empty()
{
    return { qstring_match<detail::empty_string>::mode::include };
}

inline qstring_match<detail::non_empty_string> non_empty()
{
    return { qstring_match<detail::non_empty_string>::mode::include };
}

inline qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { qstring_match<detail::empty_or_whitespace_string>::mode::include };
}

inline qstring_match<detail::email_like> email_like()
{
    return { qstring_match<detail::email_like>::mode::include };
}

inline qstring_match<detail::uri_like> uri_like()
{
    return { qstring_match<detail::uri_like>::mode::include };
}

inline qregex_checker regex(QString pattern)
{
    return { std::move(pattern), qregex_checker::mode::include };
}
} // namespace vd::qt::string_rules

#endif // VD_QSTRING_HXX
