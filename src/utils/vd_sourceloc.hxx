#pragma once

#ifndef VD_SOURCELOCATION_HXX
#define VD_SOURCELOCATION_HXX

#include <source_location>

namespace vd
{
/// @brief shortcut to std::source_location::current to avoid repeating a long source_location calls inside assertions
/// @code{.cpp}
/// vd::require(my_value == true, "my value should be a true, but got {}", vd::here(), my_value);
/// @endcode
/// @param real_location
/// @return
consteval std::source_location here(std::source_location real_location = std::source_location::current()) noexcept
{
    return real_location;
}
} // namespace vd

#endif