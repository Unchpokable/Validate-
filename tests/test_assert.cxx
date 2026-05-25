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
