#include "models/vd_string_rules.hxx"

#include "assert/vd_assert.hxx"

namespace vd::string_rules::detail
{
} // namespace vd::string_rules::detail

namespace vd::string_rules
{
bool regex_checker::operator()(std::string_view s) const
{
    bool matched = std::regex_match(std::string(s), pattern);
    return match_mode == mode::include ? matched : !matched;
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
