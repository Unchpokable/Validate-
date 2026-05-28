#pragma once

#ifndef VD_CORE_RESULT_HXX
#define VD_CORE_RESULT_HXX

#include <string>
#include <vector>

namespace vd
{
struct result {
    bool is_valid;
    std::vector<std::string> failed_rules;

    result(bool valid);

    result(bool valid, std::vector<std::string> failed_rules);

    std::string format() const;

    void with_other(const result& other);

    void die_if_failed() const;

    static result ok();

    static result failed(std::vector<std::string> failed_rules);

    operator bool() const;
};
} // namespace vd

#endif // VD_CORE_RESULT_HXX