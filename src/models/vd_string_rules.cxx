#include "models/vd_string_rules.hxx"

#include "assert/vd_assert.hxx"

namespace vd::string_rules::detail
{
} // namespace vd::string_rules::detail

namespace vd::string_rules
{
vd::result regex_checker::operator()(std::string_view s) const
{
    bool matched = std::regex_match(std::string(s), pattern);
    bool passes = (match_mode == mode::include) ? matched : !matched;
    if(passes)
        return vd::result::ok();
    if(match_mode == mode::include)
        return vd::result::failed({ "string does not match required pattern" });
    else
        return vd::result::failed({ "string matches excluded pattern" });
}

regex_checker regex(std::string_view pattern)
{
    try {
        auto regex = std::regex(pattern.begin(), pattern.end());
        return { std::move(regex), regex_checker::mode::include };
    }
    catch(const std::regex_error& e) {
        vd::require(false, "Invalid regex pattern: {}. Error code: {}", pattern, static_cast<int>(e.code()));
        return { std::regex(), regex_checker::mode::include }; // Unreachable, but satisfies return type.
    }
}
} // namespace vd::string_rules
