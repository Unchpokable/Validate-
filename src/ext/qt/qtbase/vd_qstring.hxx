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
#include "core/vd_exception.hxx"
#include "core/vd_result.hxx"

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
    std::string check_description = "string check failed";

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m) : match_mode(m)
    {
    }
    constexpr qstring_match(mode m, std::string_view desc) : match_mode(m), check_description(desc)
    {
    }

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
// QRegularExpression is compiled once at construction via the regex() factory.
struct qregex_checker {
    enum class mode { include, exclude };
    QRegularExpression re;
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

// Filters strings with size longer than given max length. WARN: QStringView::size() counts UTF-16 code units, not user-perceived
// characters — and not the same unit as the std::string_view overload (UTF-8/byte code units), so length limits are not portable
// between the two.
struct max_length_t final {
    std::size_t max_len;

    constexpr max_length_t(std::size_t max_len) : max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
    }

    vd::result operator()(QStringView s) const
    {
        if(static_cast<std::size_t>(s.size()) > max_len) {
            return vd::result::failed({ "string length exceeds the maximum allowed" });
        }
        return vd::result::ok();
    }
};

// Filters strings with size shorter than given min length. WARN: QStringView::size() counts UTF-16 code units, not user-perceived
// characters — and not the same unit as the std::string_view overload (UTF-8/byte code units), so length limits are not portable
// between the two.
struct min_length_t final {
    std::size_t min_len;

    constexpr min_length_t(std::size_t min_len) : min_len(min_len)
    {
        vd::ct_require<vd::assertion_exception>(min_len > 0, "min_len must be positive");
    }

    vd::result operator()(QStringView s) const
    {
        if(static_cast<std::size_t>(s.size()) < min_len) {
            return vd::result::failed({ "string length is less than the minimum allowed" });
        }
        return vd::result::ok();
    }
};

// Filters strings with size outside the given [min, max] range. WARN: QStringView::size() counts UTF-16 code units, not
// user-perceived characters — and not the same unit as the std::string_view overload (UTF-8/byte code units), so length limits
// are not portable between the two.
struct length_in_between_t final {
    std::size_t min_len;
    std::size_t max_len;

    constexpr length_in_between_t(std::size_t min_len, std::size_t max_len) : min_len(min_len), max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(min_len > 0, "min_len must be positive");
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
        vd::ct_require<vd::assertion_exception>(max_len >= min_len, "max_len must be greater than or equal to min_len");
    }

    vd::result operator()(QStringView s) const
    {
        if(static_cast<std::size_t>(s.size()) < min_len || static_cast<std::size_t>(s.size()) > max_len) {
            return vd::result::failed({ "string length is not within the specified range" });
        }
        return vd::result::ok();
    }
};
} // namespace vd::qt::string_rules::detail

namespace vd::qt::string_rules
{
constexpr inline qstring_match<detail::empty_string> empty()
{
    return { qstring_match<detail::empty_string>::mode::include, "string must be empty" };
}

constexpr inline qstring_match<detail::non_empty_string> non_empty()
{
    return { qstring_match<detail::non_empty_string>::mode::include, "string must be non-empty" };
}

constexpr inline qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { qstring_match<detail::empty_or_whitespace_string>::mode::include, "string must be empty or whitespace-only" };
}

constexpr inline qstring_match<detail::email_like> email_like()
{
    return { qstring_match<detail::email_like>::mode::include, "string does not look like an email address" };
}

constexpr inline qstring_match<detail::uri_like> uri_like()
{
    return { qstring_match<detail::uri_like>::mode::include, "string does not look like a URI" };
}

qregex_checker regex(QString pattern);

// Tip: this checks a UTF-16 code unit length, which is not the same as user-perceived characters (grapheme clusters). For most
// use cases this is sufficient, but be aware of this distinction if you are validating strings in languages with complex scripts
// or emojis.
constexpr inline detail::min_length_t min_length(std::size_t min_len)
{
    return detail::min_length_t(min_len);
}

// Tip: this checks a UTF-16 code unit length, which is not the same as user-perceived characters (grapheme clusters). For most
// use cases this is sufficient, but be aware of this distinction if you are validating strings in languages with complex scripts
// or emojis.
constexpr inline detail::max_length_t max_length(std::size_t max_len)
{
    return detail::max_length_t(max_len);
}

// Tip: this checks a UTF-16 code unit length, which is not the same as user-perceived characters (grapheme clusters). For most
// use cases this is sufficient, but be aware of this distinction if you are validating strings in languages with complex scripts
// or emojis.
constexpr inline detail::length_in_between_t length_in_between(std::size_t min_len, std::size_t max_len)
{
    return detail::length_in_between_t(min_len, max_len);
}
} // namespace vd::qt::string_rules

#endif // VD_QSTRING_HXX
