#pragma once

#ifndef VD_QSTRING_HXX
#define VD_QSTRING_HXX

#include <concepts>
#include <type_traits>

#include <QRegularExpression>
#include <QString>
#include <QStringView>

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

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m) : match_mode(m)
    {
    }

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
qstring_match<detail::empty_string> empty();
qstring_match<detail::non_empty_string> non_empty();
qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace();
qstring_match<detail::email_like> email_like();
qstring_match<detail::uri_like> uri_like();
qregex_checker regex(QString pattern);
} // namespace vd::qt::string_rules

#endif // VD_QSTRING_HXX
