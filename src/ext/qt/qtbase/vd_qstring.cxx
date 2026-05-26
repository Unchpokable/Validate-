#include "ext/qt/qtbase/vd_qstring.hxx"

#include "assert/vd_assert.hxx"

#include "inline_deps/ctre.hpp"

namespace vd::qt::string_rules::detail
{
bool empty_string(QStringView s)
{
    return s.isEmpty();
}

bool non_empty_string(QStringView s)
{
    return !s.isEmpty();
}

bool empty_or_whitespace_string(QStringView s)
{
    if(s.isEmpty()) {
        return true;
    }
    for(QChar c : s) {
        if(!c.isSpace()) {
            return false;
        }
    }
    return true;
}

bool email_like(QStringView s)
{
    auto utf8 = s.toUtf8();
    constexpr auto pattern = ctll::fixed_string { R"(^\S+@\S+\.\S+$)" };
    return ctre::match<pattern>(std::string_view(utf8.data(), utf8.size()));
}

bool uri_like(QStringView s)
{
    auto utf8 = s.toUtf8();
    constexpr auto pattern = ctll::fixed_string { R"(^\w+://\S+$)" };
    return ctre::match<pattern>(std::string_view(utf8.data(), utf8.size()));
}

bool qregex(QStringView s, const QString& pattern)
{
    QRegularExpression re(pattern);
    if(!re.isValid()) {
        vd::require(false, "Invalid regex pattern: {}", pattern.toStdString());
        return false;
    }
    QRegularExpressionMatch m = re.match(s);
    return m.hasMatch() && m.capturedStart() == 0 && m.capturedLength() == s.size();
}
} // namespace vd::qt::string_rules::detail

namespace vd::qt::string_rules
{
bool qregex_checker::operator()(QStringView s) const
{
    bool matched = detail::qregex(s, pattern);
    return match_mode == mode::include ? matched : !matched;
}

qstring_match<detail::empty_string> empty()
{
    return { qstring_match<detail::empty_string>::mode::include };
}

qstring_match<detail::non_empty_string> non_empty()
{
    return { qstring_match<detail::non_empty_string>::mode::include };
}

qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace()
{
    return { qstring_match<detail::empty_or_whitespace_string>::mode::include };
}

qstring_match<detail::email_like> email_like()
{
    return { qstring_match<detail::email_like>::mode::include };
}

qstring_match<detail::uri_like> uri_like()
{
    return { qstring_match<detail::uri_like>::mode::include };
}

qregex_checker regex(QString pattern)
{
    return { std::move(pattern), qregex_checker::mode::include };
}
} // namespace vd::qt::string_rules
