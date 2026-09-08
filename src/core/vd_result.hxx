#pragma once

#ifndef VD_CORE_RESULT_HXX
#define VD_CORE_RESULT_HXX

#include <initializer_list>
#include <string>
#include <vector>

namespace vd
{
struct _error_list final {
    _error_list(const char* message) : m_messages { message }
    {
    }

    _error_list(std::string message) : m_messages { std::move(message) }
    {
    }

    _error_list(std::vector<std::string> messages) : m_messages(std::move(messages))
    {
    }

    _error_list(std::initializer_list<std::string> messages) : m_messages(messages)
    {
    }

    const std::vector<std::string>& messages() const noexcept
    {
        return m_messages;
    }

private:
    std::vector<std::string> m_messages;
};

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

    static result failed(_error_list failed_rules);

    explicit operator bool() const;
};
} // namespace vd

#endif // VD_CORE_RESULT_HXX
