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

} // namespace vd::qt::string_rules::detail

namespace vd::qt::string_rules
{
vd::result qregex_checker::operator()(QStringView s) const
{
    QRegularExpressionMatch m = re.matchView(s);
    bool matched = m.hasMatch() && m.capturedStart() == 0 && m.capturedLength() == s.size();
    bool passes = (match_mode == mode::include) ? matched : !matched;
    if(passes)
        return vd::result::ok();
    if(match_mode == mode::include)
        return vd::result::failed({ "string does not match required pattern" });
    else
        return vd::result::failed({ "string matches excluded pattern" });
}

qregex_checker regex(QString pattern)
{
    QRegularExpression re(pattern);
    vd::require(re.isValid(), "Invalid regex pattern: {}", pattern.toStdString());
    return { std::move(re), qregex_checker::mode::include };
}
} // namespace vd::qt::string_rules
