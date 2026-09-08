#pragma once

#ifndef VD_SOURCELOCATION_HXX
#define VD_SOURCELOCATION_HXX

#include <source_location>

namespace vd
{
/// @brief shortcut to std::source_location::current to avoid repeating a long source_location calls inside assertions
consteval std::source_location here(std::source_location real_location = std::source_location::current()) noexcept
{
    return real_location;
}
} // namespace vd

// Opt-in only: a consumer that already ships its own std::source_location formatter
// must not collide with ours, so the specialization exists solely on request.
#if defined(VD_BUNDLE_SOURCELOCATION_FORMATTER)

// <version> first: __cpp_lib_format is not guaranteed by <source_location> above.
// MSVC happens to define every __cpp_lib_* macro in yvals_core.h, but libstdc++ and
// libc++ do not, so testing the macro before this include would silently drop the
// formatter on those implementations - the worst possible failure for an opt-in.
#include <version>

#if defined(__cpp_lib_format)

#include <format>
#include <string_view>

template<>
struct std::formatter<std::source_location> : std::formatter<std::string_view> {
    // Must be const-qualified: [formatter.requirements] requires cf.format(t, fc) to be
    // valid for a const formatter, and std::format really does call it through a const
    // lvalue - a non-const format() makes the type unusable rather than merely awkward.
    template<typename FormatContext>
    auto format(const std::source_location& loc, FormatContext& ctx) const
    {
        return std::formatter<std::string_view>::format(std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.column()), ctx);
    }
};

#endif // __cpp_lib_format
#endif // VD_BUNDLE_SOURCELOCATION_FORMATTER

#endif
