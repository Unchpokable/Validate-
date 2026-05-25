#include <gtest/gtest.h>
#include <vd.hxx>

// --- vd::require ---

TEST(RequireTest, TrueConditionDoesNotAbort)
{
    EXPECT_NO_FATAL_FAILURE(vd::require(true, "should not fire"));
}

TEST(RequireTest, TrueConditionWithArgsDoesNotAbort)
{
    EXPECT_NO_FATAL_FAILURE(vd::require(true, "value={}", 42));
}

TEST(RequireDeathTest, FalseConditionAborts)
{
    EXPECT_DEATH(vd::require(false, "boom"), "boom");
}

TEST(RequireDeathTest, FalseConditionFormatsMessage)
{
    EXPECT_DEATH(vd::require(false, "val={} str={}", 7, "hi"), "val=7 str=hi");
}

TEST(RequireDeathTest, OutputContainsAssertionFailed)
{
    EXPECT_DEATH(vd::require(false, "oops"), "Assertion failed");
}

TEST(RequireDeathTest, OutputContainsFilename)
{
    EXPECT_DEATH(vd::require(false, "loc check"), "test_assert");
}

// --- vd::require<ExceptionType> ---

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
    EXPECT_NO_THROW([]{ try { vd::require<std::logic_error>(false, "x"); } catch (const std::runtime_error&) { throw; } catch (...) {} }());
}

TEST(RequireExceptionTest, ExceptionMessageMatchesFormat)
{
    try {
        vd::require<std::runtime_error>(false, "val={} str={}", 7, "hi");
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "val=7 str=hi");
    }
}

TEST(RequireExceptionTest, ExceptionMessageNoArgs)
{
    try {
        vd::require<std::runtime_error>(false, "plain message");
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "plain message");
    }
}

TEST(RequireExceptionTest, WorksWithCustomException)
{
    struct MyError : std::exception {
        std::string msg;
        explicit MyError(std::string s) : msg(std::move(s)) {}
        const char* what() const noexcept override { return msg.c_str(); }
    };

    try {
        vd::require<MyError>(false, "custom={}", 99);
        FAIL() << "expected MyError";
    } catch (const MyError& e) {
        EXPECT_STREQ(e.what(), "custom=99");
    }
}

TEST(RequireExceptionTest, CaughtAsBaseStdException)
{
    EXPECT_THROW(vd::require<std::runtime_error>(false, "base"), std::exception);
}

// --- vd::require_callback ---

static bool s_called = false;
static std::string s_msg;

void on_fail(std::string msg)
{
    s_called = true;
    s_msg = std::move(msg);
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
    vd::require_callback<on_fail>(true, "no fire");
    EXPECT_FALSE(s_called);
}

TEST_F(RequireCallbackTest, FalseConditionInvokesCallback)
{
    vd::require_callback<on_fail>(false, "triggered");
    EXPECT_TRUE(s_called);
}

TEST_F(RequireCallbackTest, FalseConditionPassesFormattedMessage)
{
    vd::require_callback<on_fail>(false, "x={} y={}", 1, 2);
    EXPECT_EQ(s_msg, "x=1 y=2");
}

TEST_F(RequireCallbackTest, TrueConditionLeavesMessageEmpty)
{
    vd::require_callback<on_fail>(true, "x={}", 99);
    EXPECT_TRUE(s_msg.empty());
}
