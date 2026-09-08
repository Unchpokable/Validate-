#include "assert/vd_assert.hxx"

#include <cstdio>
#include <cstdlib>

#if defined(__cpp_lib_print)
#include <print>
#endif

namespace vd::detail
{
[[nodiscard]] std::string format_fail(std::string_view message, const std::source_location& loc)
{
    return std::format("Assertion failed: {}\nFile: {}\nLine: {}\nFunction: {}",
        message,
        loc.file_name(),
        static_cast<int>(loc.line()),
        loc.function_name());
}

// Both branches print exactly what format_fail produces, so the text on stderr can
// never drift apart from the text carried by assertion_exception::what().
// std::println is C++23, hence the fallback; fputs is used instead of fprintf("%s")
// because the decorated text is a std::string here, not a format template.
[[noreturn]] void assert_fail(std::string_view message, const std::source_location& loc)
{
    const std::string text = format_fail(message, loc);

#if defined(__cpp_lib_print)
    std::println(stderr, "{}", text);
#else
    std::fputs(text.c_str(), stderr);
    std::fputc('\n', stderr);
#endif

    std::abort();
}
} // namespace vd::detail
