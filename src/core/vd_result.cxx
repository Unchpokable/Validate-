#include <sstream>

#include "core/vd_exception.hxx"
#include "core/vd_result.hxx"

#include "assert/vd_assert.hxx"

vd::result::result(bool valid) : is_valid(valid)
{
}

vd::result::result(bool valid, std::vector<std::string> failed_rules) : is_valid(valid), failed_rules(std::move(failed_rules))
{
}

std::string vd::result::format() const
{
    std::stringstream ss;

    std::int32_t counter;
    for(auto msg : failed_rules) {
        ss << std::format("[#{}] Fail reason: {}\n", counter, msg);
    }

    return std::format("Result fail report: \n{}", ss.str());
}

void vd::result::with_other(const vd::result& other)
{
    if(*this != other) {
        is_valid = is_valid && other.is_valid;
        failed_rules.insert(failed_rules.end(), other.failed_rules.begin(), other.failed_rules.end());
    }
}

void vd::result::die_if_failed() const
{
    vd::require<vd::validation_exception>(is_valid, "Validation failed:\n{}", format());
}

vd::result vd::result::ok()
{
    return { true };
}

vd::result vd::result::failed(std::vector<std::string> failed_rules)
{
    vd::require(!failed_rules.empty(), "Failed result must have at least one failed rule description");
    return { false, std::move(failed_rules) };
}

vd::result::operator bool() const
{
    return is_valid;
};