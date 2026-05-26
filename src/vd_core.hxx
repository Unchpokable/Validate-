#pragma once

#ifndef VD_CORE_HXX
#define VD_CORE_HXX

#include <exception>
#include <string>


namespace vd
{
template<typename Tag>
class vd_tagged_exception : public std::exception {
public:
    explicit vd_tagged_exception(std::string message) : m_message(std::move(message))
    {
    }

    const char* what() const noexcept override
    {
        return m_message.c_str();
    }

private:
    std::string m_message;
};

using validation_exception = vd_tagged_exception<struct validation_exception_tag>;
} // namespace vd

#endif
