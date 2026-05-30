#pragma once

#ifndef VD_CORE_RESULT_HXX
#define VD_CORE_RESULT_HXX

#include <string>
#include <vector>

namespace vd
{
struct result {
    bool is_valid { true };
    std::vector<std::string> failed_rules {};

    result(bool valid);

    result(bool valid, std::vector<std::string> failed_rules);

    // @brief Prints a multiline detailed report
    // following format:
    // Result Fail Report:\n
    // [#1] Field X should be in range [-10, 10]\n
    // [#2] Field Y string should not be empty\n
    std::string format() const;

    // @brief Prints a short singleline message
    // following format:
    // Field X should be in range [-10, 10], Field Y string should not be empty
    std::string short_format() const;

    void with_other(const result& other);

    result& also(const result& other) &;
    result also(const result& other) &&;

    void die_if_failed() const;

    static result ok();

    static result failed(std::vector<std::string> failed_rules);

    operator bool() const;
};
} // namespace vd

#endif // VD_CORE_RESULT_HXX
