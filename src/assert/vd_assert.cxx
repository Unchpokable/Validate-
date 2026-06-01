#include "assert/vd_assert.hxx"

namespace vd::detail
{
[[noreturn]] void assert_fail(std::string_view message, const std::source_location& loc)
{
    std::fprintf(stderr,
        "Assertion failed: %s\nFile: %s\nLine: %d\nFunction: %s\n",
        message.data(),
        loc.file_name(),
        static_cast<int>(loc.line()),
        loc.function_name());
    std::abort();
}
} // namespace vd::detail
