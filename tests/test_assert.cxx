#include <gtest/gtest.h>
#include <vd.hxx>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// The debug-only family (strict_required / required / require_cbd) is compiled out
// by the very same condition inside vd_assert.hxx, so mirror it here instead of
// assuming a Debug build: the expectations must follow whatever was actually built.
#if not defined(_NDEBUG) and not defined(NDEBUG) and not defined(RELEASE)
#define VD_TEST_DEBUG_ASSERTS_ACTIVE 1
#else
#define VD_TEST_DEBUG_ASSERTS_ACTIVE 0
#endif

namespace
{
bool contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string_view::npos;
}

// Every failing flavour funnels its message through detail::format_fail, so the whole
// family shares one decoration shape: the formatted body plus the three location fields.
::testing::AssertionResult is_decorated(std::string_view text, std::string_view body)
{
    const std::string expected_body = "Assertion failed: " + std::string(body);
    for(const std::string& part : { expected_body, std::string("File: "), std::string("Line: "), std::string("Function: ") }) {
        if(!contains(text, part)) {
            return ::testing::AssertionFailure() << "missing \"" << part << "\" in:\n" << text;
        }
    }
    return ::testing::AssertionSuccess();
}

// Runs fn and returns what() of the expected exception, or "" if nothing was thrown.
template<typename ExceptionType, typename Fn>
std::string thrown_message(Fn&& fn)
{
    try {
        fn();
    }
    catch(const ExceptionType& e) {
        return e.what();
    }
    return {};
}

struct my_error : std::exception {
    std::string msg;

    explicit my_error(std::string s) : msg(std::move(s))
    {
    }

    const char* what() const noexcept override
    {
        return msg.c_str();
    }
};
} // namespace

// ---------------------------------------------------------------------------
// detail::contextually_bool — the concept every require flavour is gated on
// ---------------------------------------------------------------------------

namespace
{
struct explicit_bool {
    bool value;

    explicit operator bool() const
    {
        return value;
    }
};

struct no_bool {};

// The condition is taken by const&, so a non-const conversion operator must be rejected
// by the concept — with a clean "constraint not satisfied", not an error inside the body.
struct non_const_bool {
    explicit operator bool()
    {
        return true;
    }
};

// Counts copies to prove the condition is bound, not copied.
struct copy_counter {
    static inline int copies = 0;
    bool value;

    explicit copy_counter(bool v) : value(v)
    {
    }

    copy_counter(const copy_counter& other) : value(other.value)
    {
        ++copies;
    }

    explicit operator bool() const
    {
        return value;
    }
};

// static_cast<bool> is fine here, but `!t` yields void — the concept requires both.
struct half_bool {
    explicit operator bool() const
    {
        return true;
    }

    void operator!() const
    {
    }
};
} // namespace

static_assert(vd::detail::contextually_bool<bool>);
static_assert(vd::detail::contextually_bool<int>);
static_assert(vd::detail::contextually_bool<int*>);
static_assert(vd::detail::contextually_bool<std::optional<int>>);
static_assert(vd::detail::contextually_bool<std::shared_ptr<int>>);
static_assert(vd::detail::contextually_bool<explicit_bool>);
static_assert(vd::detail::contextually_bool<vd::result>);
static_assert(!vd::detail::contextually_bool<no_bool>);
static_assert(!vd::detail::contextually_bool<std::string>);
static_assert(!vd::detail::contextually_bool<half_bool>);
static_assert(!vd::detail::contextually_bool<non_const_bool>);
static_assert(vd::detail::contextually_bool<std::unique_ptr<int>>);

TEST(RequireConditionTypesTest, AcceptsRawPointer)
{
    int value = 0;
    EXPECT_NO_THROW(vd::require(&value, "pointer must be non-null"));
    EXPECT_THROW(vd::require(static_cast<int*>(nullptr), "pointer must be non-null"), vd::assertion_exception);
}

TEST(RequireConditionTypesTest, AcceptsOptional)
{
    EXPECT_NO_THROW(vd::require(std::optional<int> { 1 }, "optional must be engaged"));
    EXPECT_THROW(vd::require(std::optional<int> {}, "optional must be engaged"), vd::assertion_exception);
}

TEST(RequireConditionTypesTest, AcceptsSharedPtr)
{
    EXPECT_NO_THROW(vd::require(std::make_shared<int>(7), "handle must be alive"));
    EXPECT_THROW(vd::require(std::shared_ptr<int> {}, "handle must be alive"), vd::assertion_exception);
}

TEST(RequireConditionTypesTest, AcceptsExplicitOperatorBool)
{
    EXPECT_NO_THROW(vd::require(explicit_bool { true }, "explicit bool"));
    EXPECT_THROW(vd::require(explicit_bool { false }, "explicit bool"), vd::assertion_exception);
}

// const Cond& binds a move-only lvalue: taking the condition by value would not compile here.
TEST(RequireConditionTypesTest, AcceptsMoveOnlyLvalue)
{
    std::unique_ptr<int> owned = std::make_unique<int>(3);
    EXPECT_NO_THROW(vd::require(owned, "owner must hold something"));
    EXPECT_TRUE(owned); // still owns it — nothing was moved out

    owned.reset();
    EXPECT_THROW(vd::require(owned, "owner must hold something"), vd::assertion_exception);
}

TEST(RequireConditionTypesTest, ConditionIsNotCopied)
{
    copy_counter::copies = 0;
    const copy_counter cond { true };

    vd::require(cond, "no copy");
    vd::require<std::runtime_error>(cond, "no copy");
    vd::strict_require(cond, "no copy");

    EXPECT_EQ(copy_counter::copies, 0);
}

// vd::result models contextually_bool, so a check() outcome can be fed to require directly.
TEST(RequireConditionTypesTest, AcceptsVdResult)
{
    EXPECT_NO_THROW(vd::require(vd::result::ok(), "result must be ok"));
    EXPECT_THROW(vd::require(vd::result::failed("nope"), "result must be ok"), vd::assertion_exception);
}

// ---------------------------------------------------------------------------
// vd::strict_require — the abort() flavour
// ---------------------------------------------------------------------------

TEST(StrictRequireTest, TrueConditionDoesNotAbort)
{
    EXPECT_NO_FATAL_FAILURE(vd::strict_require(true, "should not fire"));
}

TEST(StrictRequireTest, TrueConditionWithArgsDoesNotAbort)
{
    EXPECT_NO_FATAL_FAILURE(vd::strict_require(true, "value={}", 42));
}

TEST(StrictRequireTest, TrueConditionDoesNotThrowEither)
{
    EXPECT_NO_THROW(vd::strict_require(true, "should not fire"));
}

TEST(StrictRequireDeathTest, FalseConditionAborts)
{
    EXPECT_DEATH(vd::strict_require(false, "boom"), "boom");
}

TEST(StrictRequireDeathTest, FalseConditionFormatsMessage)
{
    EXPECT_DEATH(vd::strict_require(false, "val={} str={}", 7, "hi"), "val=7 str=hi");
}

TEST(StrictRequireDeathTest, OutputContainsAssertionFailed)
{
    EXPECT_DEATH(vd::strict_require(false, "oops"), "Assertion failed");
}

TEST(StrictRequireDeathTest, OutputContainsFilename)
{
    EXPECT_DEATH(vd::strict_require(false, "loc check"), "test_assert");
}

TEST(StrictRequireDeathTest, OutputContainsFunctionName)
{
    EXPECT_DEATH(vd::strict_require(false, "func check"), "TestBody");
}

TEST(StrictRequireDeathTest, OutputContainsLineLabel)
{
    EXPECT_DEATH(vd::strict_require(false, "line check"), "Line: ");
}

// ---------------------------------------------------------------------------
// vd::require — throws vd::assertion_exception carrying the decorated message
// ---------------------------------------------------------------------------

TEST(RequireTest, TrueConditionDoesNotThrow)
{
    EXPECT_NO_THROW(vd::require(true, "should not fire"));
}

TEST(RequireTest, TrueConditionWithArgsDoesNotThrow)
{
    EXPECT_NO_THROW(vd::require(true, "value={}", 42));
}

TEST(RequireTest, FalseConditionThrowsAssertionException)
{
    EXPECT_THROW(vd::require(false, "boom"), vd::assertion_exception);
}

TEST(RequireTest, FalseConditionDoesNotAbort)
{
    // Regression guard for the require/strict_require split: the plain flavour
    // must unwind, not kill the process.
    EXPECT_NO_FATAL_FAILURE({
        try {
            vd::require(false, "must unwind");
        }
        catch(const vd::assertion_exception&) {
        }
    });
}

TEST(RequireTest, CaughtAsBaseStdException)
{
    EXPECT_THROW(vd::require(false, "base"), std::exception);
}

TEST(RequireTest, MessageContainsFormattedText)
{
    const auto msg = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "val={} str={}", 7, "hi");
    });
    EXPECT_TRUE(contains(msg, "val=7 str=hi")) << msg;
}

TEST(RequireTest, MessageIsDecoratedWithAssertionFailedPrefix)
{
    const auto msg = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "oops");
    });
    EXPECT_TRUE(contains(msg, "Assertion failed: oops")) << msg;
}

TEST(RequireTest, MessageContainsSourceLocation)
{
    const auto msg = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "loc check");
    });
    EXPECT_TRUE(contains(msg, "File: ")) << msg;
    EXPECT_TRUE(contains(msg, "test_assert")) << msg;
    EXPECT_TRUE(contains(msg, "Line: ")) << msg;
    EXPECT_TRUE(contains(msg, "Function: ")) << msg;
}

// assert_format's consteval ctor captures source_location at the *call site*,
// not inside the require template — this pins that down.
TEST(RequireTest, ReportsCallSiteLine)
{
    const int expected_line = __LINE__ + 2;
    const auto msg = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "line probe");
    });
    EXPECT_TRUE(contains(msg, "Line: " + std::to_string(expected_line))) << msg;
}

// assertion_exception and validation_exception are distinct tag instantiations
// of vd_tagged_exception, so they must not cross-catch.
TEST(RequireTest, AssertionExceptionDoesNotCollideWithValidationException)
{
    bool caught_as_validation = false;
    bool caught_as_assertion = false;

    try {
        vd::require(false, "tagged");
    }
    catch(const vd::validation_exception&) {
        caught_as_validation = true;
    }
    catch(const vd::assertion_exception&) {
        caught_as_assertion = true;
    }

    EXPECT_FALSE(caught_as_validation);
    EXPECT_TRUE(caught_as_assertion);
}

TEST(RequireTest, ConditionIsEvaluatedOnlyOnce)
{
    int calls = 0;
    auto probe = [&calls] {
        ++calls;
        return true;
    };

    vd::require(probe(), "no fire");
    EXPECT_EQ(calls, 1);
}

TEST(RequireTest, FormatArgumentsAreEvaluatedEagerly)
{
    // Format args are ordinary function arguments: they are evaluated even when the
    // condition holds. Documented behaviour, worth locking in.
    int calls = 0;
    auto probe = [&calls] {
        ++calls;
        return 1;
    };

    vd::require(true, "value={}", probe());
    EXPECT_EQ(calls, 1);
}

// ---------------------------------------------------------------------------
// vd::require<ExceptionType> — throws the requested type, message undecorated
// ---------------------------------------------------------------------------

TEST(RequireExceptionTest, TrueConditionDoesNotThrow)
{
    EXPECT_NO_THROW(vd::require<std::runtime_error>(true, "should not fire"));
}

TEST(RequireExceptionTest, TrueConditionWithArgsDoesNotThrow)
{
    EXPECT_NO_THROW(vd::require<std::runtime_error>(true, "value={}", 42));
}

TEST(RequireExceptionTest, FalseConditionThrows)
{
    EXPECT_THROW(vd::require<std::runtime_error>(false, "boom"), std::runtime_error);
}

TEST(RequireExceptionTest, ThrowsCorrectExceptionType)
{
    EXPECT_THROW(vd::require<std::logic_error>(false, "logic"), std::logic_error);

    bool caught_as_runtime = false;
    try {
        vd::require<std::logic_error>(false, "x");
    }
    catch(const std::runtime_error&) {
        caught_as_runtime = true;
    }
    catch(const std::logic_error&) {
    }
    EXPECT_FALSE(caught_as_runtime);
}

TEST(RequireExceptionTest, ExceptionMessageMatchesFormat)
{
    try {
        vd::require<std::runtime_error>(false, "val={} str={}", 7, "hi");
        FAIL() << "expected std::runtime_error";
    }
    catch(const std::runtime_error& e) {
        EXPECT_TRUE(is_decorated(e.what(), "val=7 str=hi"));
    }
}

TEST(RequireExceptionTest, ExceptionMessageNoArgs)
{
    try {
        vd::require<std::runtime_error>(false, "plain message");
        FAIL() << "expected std::runtime_error";
    }
    catch(const std::runtime_error& e) {
        EXPECT_TRUE(is_decorated(e.what(), "plain message"));
    }
}

// The typed overload runs through format_fail as well — the decoration is uniform
// across the whole family, only the thrown type differs.
TEST(RequireExceptionTest, MessageIsDecoratedWithSourceLocation)
{
    const auto msg = thrown_message<std::runtime_error>([] {
        vd::require<std::runtime_error>(false, "clean");
    });
    EXPECT_TRUE(is_decorated(msg, "clean"));
    EXPECT_TRUE(contains(msg, "test_assert")) << msg;
}

// Asking explicitly for assertion_exception picks the typed overload (ExceptionType is
// the first explicit template parameter, so it wins over the one-arg form) — and the
// resulting message must be indistinguishable from what plain require produces.
TEST(RequireExceptionTest, ExplicitAssertionExceptionMatchesPlainRequireShape)
{
    const auto typed = thrown_message<vd::assertion_exception>([] {
        vd::require<vd::assertion_exception>(false, "explicit tag");
    });
    const auto plain = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "explicit tag");
    });

    EXPECT_TRUE(is_decorated(typed, "explicit tag"));
    // Everything up to the line number is identical: same body, same file.
    EXPECT_EQ(typed.substr(0, typed.find("\nLine:")), plain.substr(0, plain.find("\nLine:")));
}

TEST(RequireExceptionTest, WorksWithValidationException)
{
    const auto msg = thrown_message<vd::validation_exception>([] {
        vd::require<vd::validation_exception>(false, "field {} invalid", "age");
    });
    EXPECT_TRUE(is_decorated(msg, "field age invalid"));
}

TEST(RequireExceptionTest, WorksWithCustomException)
{
    try {
        vd::require<my_error>(false, "custom={}", 99);
        FAIL() << "expected my_error";
    }
    catch(const my_error& e) {
        EXPECT_TRUE(is_decorated(e.what(), "custom=99"));
    }
}

TEST(RequireExceptionTest, CaughtAsBaseStdException)
{
    EXPECT_THROW(vd::require<std::runtime_error>(false, "base"), std::exception);
}

// ---------------------------------------------------------------------------
// vd::ct_require<ExceptionType> — constexpr-friendly throwing check
// ---------------------------------------------------------------------------

namespace
{
constexpr int checked_div(int a, int b)
{
    vd::ct_require<std::logic_error>(b != 0, "division by zero: {}/{}", a, b);
    return a / b;
}

constexpr std::size_t checked_len(std::size_t max_len)
{
    vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
    return max_len;
}

// A failing ct_require makes the enclosing call a non-constant expression, which is
// exactly how the string_rules factories turn bad arguments into compile-time errors.
template<int A, int B>
concept constant_divisible = requires { std::integral_constant<int, checked_div(A, B)> {}; };
} // namespace

static_assert(checked_div(10, 2) == 5, "ct_require must stay usable during constant evaluation");
static_assert(checked_len(4) == 4);
static_assert(constant_divisible<10, 2>);
static_assert(!constant_divisible<10, 0>, "a failed ct_require must break constant evaluation");

TEST(CtRequireTest, ConstantEvaluationPassesThrough)
{
    constexpr int value = checked_div(9, 3);
    EXPECT_EQ(value, 3);
}

TEST(CtRequireTest, RuntimeFalseConditionThrows)
{
    int zero = 0; // runtime value: the check degrades to an ordinary throwing guard
    EXPECT_THROW((void)checked_div(1, zero), std::logic_error);
}

TEST(CtRequireTest, RuntimeTrueConditionDoesNotThrow)
{
    int two = 2;
    EXPECT_NO_THROW((void)checked_div(4, two));
}

TEST(CtRequireTest, MessageIsFormattedAndDecorated)
{
    int zero = 0;
    const auto msg = thrown_message<std::logic_error>([zero] {
        (void)checked_div(1, zero);
    });
    EXPECT_TRUE(is_decorated(msg, "division by zero: 1/0"));
    // The location points at the guard inside checked_div, not at this test.
    EXPECT_TRUE(contains(msg, "checked_div")) << msg;
}

TEST(CtRequireTest, WorksWithAssertionException)
{
    std::size_t zero = 0;
    EXPECT_THROW((void)checked_len(zero), vd::assertion_exception);
}

// ---------------------------------------------------------------------------
// vd::require_cb — hands the formatted message to a compile-time callback
// ---------------------------------------------------------------------------

static bool s_called = false;
static std::string s_msg;

void on_fail(std::string_view msg)
{
    s_called = true;
    s_msg.assign(msg);
}

class RequireCallbackTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        s_called = false;
        s_msg.clear();
    }
};

TEST_F(RequireCallbackTest, TrueConditionDoesNotInvokeCallback)
{
    vd::require_cb<on_fail>(true, "no fire");
    EXPECT_FALSE(s_called);
}

TEST_F(RequireCallbackTest, FalseConditionInvokesCallback)
{
    vd::require_cb<on_fail>(false, "triggered");
    EXPECT_TRUE(s_called);
}

TEST_F(RequireCallbackTest, FalseConditionPassesFormattedMessage)
{
    vd::require_cb<on_fail>(false, "x={} y={}", 1, 2);
    EXPECT_TRUE(is_decorated(s_msg, "x=1 y=2"));
}

// The callback receives the same decorated text the throwing flavours carry, so a
// collected message is self-describing without the caller re-adding context.
TEST_F(RequireCallbackTest, MessageIsDecoratedWithSourceLocation)
{
    vd::require_cb<on_fail>(false, "plain");
    EXPECT_TRUE(is_decorated(s_msg, "plain"));
    EXPECT_TRUE(contains(s_msg, "test_assert")) << s_msg;
}

TEST_F(RequireCallbackTest, TrueConditionLeavesMessageEmpty)
{
    vd::require_cb<on_fail>(true, "x={}", 99);
    EXPECT_TRUE(s_msg.empty());
}

TEST_F(RequireCallbackTest, NeitherThrowsNorAborts)
{
    EXPECT_NO_FATAL_FAILURE(vd::require_cb<on_fail>(false, "soft failure"));
    EXPECT_NO_THROW(vd::require_cb<on_fail>(false, "soft failure"));
}

TEST_F(RequireCallbackTest, AcceptsCapturelessLambdaAsCallback)
{
    // A captureless closure decays to a function pointer, which is a valid `auto` NTTP.
    vd::require_cb<+[](std::string_view msg) {
        s_msg.assign(msg);
        s_called = true;
    }>(false, "from lambda {}", 5);

    EXPECT_TRUE(s_called);
    EXPECT_TRUE(is_decorated(s_msg, "from lambda 5"));
}

TEST_F(RequireCallbackTest, AcceptsNonBoolConditions)
{
    vd::require_cb<on_fail>(std::optional<int> {}, "optional was empty");
    EXPECT_TRUE(s_called);
}

// ---------------------------------------------------------------------------
// Debug-only family: strict_required / required / required<E> / require_cbd
// ---------------------------------------------------------------------------

TEST(DebugAssertsTest, TrueConditionsNeverFire)
{
    EXPECT_NO_THROW(vd::strict_required(true, "no fire"));
    EXPECT_NO_THROW(vd::required(true, "no fire"));
    EXPECT_NO_THROW(vd::required<std::runtime_error>(true, "no fire"));
}

// Compile-coverage for the release no-op overloads: they must keep the same const& signature,
// otherwise a move-only condition would build in Debug and break in Release.
TEST_F(RequireCallbackTest, DebugFamilyAcceptsMoveOnlyLvalue)
{
    std::unique_ptr<int> owned = std::make_unique<int>(1);

    EXPECT_NO_THROW(vd::strict_required(owned, "still alive"));
    EXPECT_NO_THROW(vd::required(owned, "still alive"));
    EXPECT_NO_THROW(vd::required<std::runtime_error>(owned, "still alive"));
    vd::require_cbd<on_fail>(owned, "still alive");

    EXPECT_FALSE(s_called);
    EXPECT_TRUE(owned);
}

TEST_F(RequireCallbackTest, DebugCallbackNotInvokedOnTrueCondition)
{
    vd::require_cbd<on_fail>(true, "no fire");
    EXPECT_FALSE(s_called);
}

#if VD_TEST_DEBUG_ASSERTS_ACTIVE

TEST(DebugAssertsTest, RequiredThrowsAssertionException)
{
    EXPECT_THROW(vd::required(false, "debug boom"), vd::assertion_exception);
}

TEST(DebugAssertsTest, RequiredMessageIsDecorated)
{
    const auto msg = thrown_message<vd::assertion_exception>([] {
        vd::required(false, "debug {}", "boom");
    });
    EXPECT_TRUE(contains(msg, "Assertion failed: debug boom")) << msg;
    EXPECT_TRUE(contains(msg, "test_assert")) << msg;
}

TEST(DebugAssertsTest, RequiredWithExceptionTypeThrowsDecorated)
{
    const auto msg = thrown_message<std::runtime_error>([] {
        vd::required<std::runtime_error>(false, "debug typed {}", 1);
    });
    EXPECT_TRUE(is_decorated(msg, "debug typed 1"));
}

TEST_F(RequireCallbackTest, DebugCallbackInvokedOnFalseCondition)
{
    vd::require_cbd<on_fail>(false, "debug cb {}", 3);
    EXPECT_TRUE(s_called);
    EXPECT_TRUE(is_decorated(s_msg, "debug cb 3"));
}

TEST(DebugAssertsDeathTest, StrictRequiredAborts)
{
    EXPECT_DEATH(vd::strict_required(false, "debug strict boom"), "debug strict boom");
}

#else

TEST(DebugAssertsTest, RequiredIsCompiledOut)
{
    EXPECT_NO_THROW(vd::required(false, "compiled out"));
    EXPECT_NO_THROW(vd::required<std::runtime_error>(false, "compiled out"));
    EXPECT_NO_FATAL_FAILURE(vd::strict_required(false, "compiled out"));
}

TEST_F(RequireCallbackTest, DebugCallbackIsCompiledOut)
{
    vd::require_cbd<on_fail>(false, "compiled out");
    EXPECT_FALSE(s_called);
}

#endif

// ---------------------------------------------------------------------------
// detail::format_fail — the shared message decorator
// ---------------------------------------------------------------------------

TEST(FormatFailTest, ContainsMessageAndEveryLocationField)
{
    const auto loc = std::source_location::current();
    const std::string text = vd::detail::format_fail("message body", loc);

    EXPECT_TRUE(contains(text, "Assertion failed: message body")) << text;
    EXPECT_TRUE(contains(text, std::string("File: ") + loc.file_name())) << text;
    EXPECT_TRUE(contains(text, "Line: " + std::to_string(loc.line()))) << text;
    EXPECT_TRUE(contains(text, std::string("Function: ") + loc.function_name())) << text;
}

// std::string_view carries a size, not a terminator: format_fail must respect it and stop
// at the view's end instead of running to the next NUL in the underlying buffer.
TEST(FormatFailTest, RespectsNonNullTerminatedStringView)
{
    const char buffer[] = "abcdefghij";
    const std::string_view view(buffer, 3);

    const std::string text = vd::detail::format_fail(view, std::source_location::current());

    EXPECT_TRUE(contains(text, "Assertion failed: abc\n")) << text;
    EXPECT_FALSE(contains(text, "abcdef")) << text;
}

// One decoration for the whole family: whichever failure channel is used, the text
// the user finally sees comes from the same format_fail call.
TEST(FormatFailTest, EveryFlavourSharesTheSameDecoration)
{
    const auto plain = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "same body");
    });
    const auto typed = thrown_message<std::runtime_error>([] {
        vd::require<std::runtime_error>(false, "same body");
    });

    s_msg.clear();
    vd::require_cb<on_fail>(false, "same body");

    for(const std::string& text : { plain, typed, s_msg }) {
        EXPECT_TRUE(is_decorated(text, "same body"));
    }
}

TEST(FormatFailTest, SharesItsPrefixWithWhatRequireThrows)
{
    const auto loc = std::source_location::current();
    const std::string direct = vd::detail::format_fail("same text", loc);

    const auto thrown = thrown_message<vd::assertion_exception>([] {
        vd::require(false, "same text");
    });

    // Same shape, different location — compare the decorated prefix only.
    EXPECT_EQ(direct.substr(0, direct.find("\nFile:")), thrown.substr(0, thrown.find("\nFile:")));
}
