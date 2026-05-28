#pragma once

#ifndef VD_CORE_RESULT_HXX
#define VD_CORE_RESULT_HXX

#include <vector>

#include "assert/vd_assert.hxx"

namespace vd
{
struct result {
    bool is_valid;
    std::vector<std::string> failed_rules;

    result(bool valid) : is_valid(valid)
    {
    }

    result(bool valid, std::vector<std::string> failed_rules) : is_valid(valid), failed_rules(std::move(failed_rules))
    {
    }

    static result ok()
    {
        return { true };
    }

    static result failed(std::vector<std::string> failed_rules)
    {
        vd::require(!failed_rules.empty(), "Failed result must have at least one failed rule description");
        return { false, std::move(failed_rules) };
    }

    explicit operator bool() const
    {
        return is_valid;
    };
};
} // namespace vd

#endif // VD_CORE_RESULT_HXX