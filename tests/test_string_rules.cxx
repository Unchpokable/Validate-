#include <gtest/gtest.h>
#include <vd.hxx>

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

struct Profile {
    std::string name;
    std::string email;
    std::string website;

    const std::string& get_name() const
    {
        return name;
    }
    const std::string& get_email() const
    {
        return email;
    }
};

// ---------------------------------------------------------------------------
// string_rules::empty
// An empty string passes; any non-empty string (including whitespace) fails.
// ---------------------------------------------------------------------------

TEST(StringRulesEmptyTest, AcceptsEmptyString)
{
    EXPECT_TRUE(vd::string_rules::empty()(""));
}

TEST(StringRulesEmptyTest, RejectsNonEmpty)
{
    EXPECT_FALSE(vd::string_rules::empty()("x"));
    EXPECT_FALSE(vd::string_rules::empty()("hello world"));
}

TEST(StringRulesEmptyTest, RejectsWhitespaceOnlyString)
{
    // Whitespace is not empty — empty() should reject it.
    EXPECT_FALSE(vd::string_rules::empty()(" "));
    EXPECT_FALSE(vd::string_rules::empty()("\t\n"));
}

// ---------------------------------------------------------------------------
// string_rules::non_empty
// Non-empty string (including whitespace) passes; only "" fails.
// ---------------------------------------------------------------------------

TEST(StringRulesNonEmptyTest, AcceptsNonEmpty)
{
    EXPECT_TRUE(vd::string_rules::non_empty()("x"));
    EXPECT_TRUE(vd::string_rules::non_empty()("hello world"));
}

TEST(StringRulesNonEmptyTest, AcceptsWhitespaceOnly)
{
    // Whitespace is not empty — non_empty() should accept it.
    EXPECT_TRUE(vd::string_rules::non_empty()(" "));
    EXPECT_TRUE(vd::string_rules::non_empty()("\t"));
}

TEST(StringRulesNonEmptyTest, RejectsEmptyString)
{
    EXPECT_FALSE(vd::string_rules::non_empty()(""));
}

// ---------------------------------------------------------------------------
// string_rules::empty_or_whitespace
// Passes for "" or strings consisting entirely of whitespace characters.
// ---------------------------------------------------------------------------

TEST(StringRulesEmptyOrWhitespaceTest, AcceptsEmptyString)
{
    EXPECT_TRUE(vd::string_rules::empty_or_whitespace()(""));
}

TEST(StringRulesEmptyOrWhitespaceTest, AcceptsAllWhitespace)
{
    EXPECT_TRUE(vd::string_rules::empty_or_whitespace()("   "));
    EXPECT_TRUE(vd::string_rules::empty_or_whitespace()("\t\n\r\f\v"));
    EXPECT_TRUE(vd::string_rules::empty_or_whitespace()("  \t  "));
}

TEST(StringRulesEmptyOrWhitespaceTest, RejectsStringWithContent)
{
    EXPECT_FALSE(vd::string_rules::empty_or_whitespace()("x"));
    EXPECT_FALSE(vd::string_rules::empty_or_whitespace()("  x  "));
    EXPECT_FALSE(vd::string_rules::empty_or_whitespace()("hello"));
}

// ---------------------------------------------------------------------------
// string_rules::email_like
// Minimal heuristic: expects \S+@\S+\.\S+
// ---------------------------------------------------------------------------

TEST(StringRulesEmailLikeTest, AcceptsTypicalEmail)
{
    EXPECT_TRUE(vd::string_rules::email_like()("user@example.com"));
    EXPECT_TRUE(vd::string_rules::email_like()("a@b.c"));
    EXPECT_TRUE(vd::string_rules::email_like()("name.surname@company.org"));
}

TEST(StringRulesEmailLikeTest, RejectsMissingAt)
{
    EXPECT_FALSE(vd::string_rules::email_like()("notanemail"));
    EXPECT_FALSE(vd::string_rules::email_like()("nodot.com"));
}

TEST(StringRulesEmailLikeTest, RejectsMissingDomain)
{
    EXPECT_FALSE(vd::string_rules::email_like()("user@"));
    EXPECT_FALSE(vd::string_rules::email_like()("user@nodot"));
}

TEST(StringRulesEmailLikeTest, RejectsEmpty)
{
    EXPECT_FALSE(vd::string_rules::email_like()(""));
}

// ---------------------------------------------------------------------------
// string_rules::uri_like
// Minimal heuristic: expects \w+://\S+
// ---------------------------------------------------------------------------

TEST(StringRulesUriLikeTest, AcceptsCommonSchemes)
{
    EXPECT_TRUE(vd::string_rules::uri_like()("https://example.com"));
    EXPECT_TRUE(vd::string_rules::uri_like()("http://localhost"));
    EXPECT_TRUE(vd::string_rules::uri_like()("ftp://files.example.com/path"));
}

TEST(StringRulesUriLikeTest, RejectsMissingScheme)
{
    EXPECT_FALSE(vd::string_rules::uri_like()("example.com"));
    EXPECT_FALSE(vd::string_rules::uri_like()("//example.com"));
}

TEST(StringRulesUriLikeTest, RejectsMissingSeparator)
{
    EXPECT_FALSE(vd::string_rules::uri_like()("httpexample.com"));
}

TEST(StringRulesUriLikeTest, RejectsEmpty)
{
    EXPECT_FALSE(vd::string_rules::uri_like()(""));
}

// ---------------------------------------------------------------------------
// string_rules::regex
// Runtime pattern matching via std::regex. regex_checker is compatible with
// value_checker because it exposes operator()(std::string_view) -> bool.
// ---------------------------------------------------------------------------

TEST(StringRulesRegexTest, MatchesLiteralPattern)
{
    auto checker = vd::string_rules::regex("hello");
    EXPECT_TRUE(checker("hello"));
    EXPECT_FALSE(checker("world"));
    EXPECT_FALSE(checker("hello world")); // regex_match requires full-string match
}

TEST(StringRulesRegexTest, MatchesDigitPattern)
{
    auto checker = vd::string_rules::regex(R"(\d+)");
    EXPECT_TRUE(checker("123"));
    EXPECT_TRUE(checker("0"));
    EXPECT_FALSE(checker("abc"));
    EXPECT_FALSE(checker("12x"));
}

TEST(StringRulesRegexTest, MatchesEmptyPattern)
{
    // An empty regex matches only the empty string (full match semantics).
    auto checker = vd::string_rules::regex("");
    EXPECT_TRUE(checker(""));
    EXPECT_FALSE(checker("x"));
}

TEST(StringRulesRegexDeathTest, InvalidPatternAborts)
{
    // An invalid regex pattern must trigger vd::require and abort.
    EXPECT_DEATH(vd::string_rules::regex("[invalid"), "");
}

// ---------------------------------------------------------------------------
// Integration: string_rules checkers with vd::basic_model via vd::member
// ---------------------------------------------------------------------------

TEST(StringRulesModelTest, MemberNonEmptyValidatesName)
{
    auto model = vd::basic_model<Profile>().with(vd::member(&Profile::name, vd::string_rules::non_empty()));

    EXPECT_TRUE(model.check(Profile { "Alice", "", "" }));
    EXPECT_FALSE(model.check(Profile { "", "", "" }));
}

TEST(StringRulesModelTest, MemberEmailLikeValidatesEmail)
{
    auto model = vd::basic_model<Profile>().with(vd::member(&Profile::email, vd::string_rules::email_like()));

    EXPECT_TRUE(model.check(Profile { "x", "user@example.com", "" }));
    EXPECT_FALSE(model.check(Profile { "x", "notanemail", "" }));
}

TEST(StringRulesModelTest, FieldGetterNonEmptyViaGetter)
{
    // Uses vd::field with a member function pointer (getter).
    auto model = vd::basic_model<Profile>().with(vd::field(&Profile::get_name, vd::string_rules::non_empty()));

    EXPECT_TRUE(model.check(Profile { "Bob", "", "" }));
    EXPECT_FALSE(model.check(Profile { "", "", "" }));
}

TEST(StringRulesModelTest, MultipleStringRulesAllMustPass)
{
    // Both name and email must be valid.
    auto model = vd::basic_model<Profile>()
                     .with(vd::member(&Profile::name, vd::string_rules::non_empty()))
                     .with(vd::member(&Profile::email, vd::string_rules::email_like()));

    EXPECT_TRUE(model.check(Profile { "Alice", "a@b.com", "" }));
    EXPECT_FALSE(model.check(Profile { "", "a@b.com", "" }));  // name empty
    EXPECT_FALSE(model.check(Profile { "Alice", "bad", "" })); // email invalid
    EXPECT_FALSE(model.check(Profile { "", "bad", "" }));      // both fail
}

TEST(StringRulesModelTest, MemberUriLikeValidatesWebsite)
{
    auto model = vd::basic_model<Profile>().with(vd::member(&Profile::website, vd::string_rules::uri_like()));

    EXPECT_TRUE(model.check(Profile { "", "", "https://example.com" }));
    EXPECT_FALSE(model.check(Profile { "", "", "not-a-uri" }));
}

TEST(StringRulesModelTest, RegexCheckerWorksWithMember)
{
    // Validate that the name consists only of word characters.
    auto model = vd::basic_model<Profile>().with(vd::member(&Profile::name, vd::string_rules::regex(R"(\w+)")));

    EXPECT_TRUE(model.check(Profile { "Alice123", "", "" }));
    EXPECT_FALSE(model.check(Profile { "Alice 123", "", "" })); // space not allowed
    EXPECT_FALSE(model.check(Profile { "", "", "" }));          // empty doesn't match \w+
}
