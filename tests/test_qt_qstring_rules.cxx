#include <gtest/gtest.h>

#include <memory>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QStringView>

#include "ext/qt/qtbase/vd_qproperty.hxx"
#include "ext/qt/qtbase/vd_qstring.hxx"
#include "models/vd_basic_model.hxx"
#include "models/vd_rule_factory.hxx"

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

// Plain struct with QString fields — used with vd::member.
struct QStringProfile {
    QString name;
    QString email;
    QString website;
};

// QObject with Q_PROPERTYs — used with vd::qt::qt_property.
class StringQObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString email READ email)
    Q_PROPERTY(QString website READ website)

public:
    explicit StringQObject(const QString& name, const QString& email, const QString& website, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_email(email), m_website(website)
    {
    }

    QString name() const
    {
        return m_name;
    }
    QString email() const
    {
        return m_email;
    }
    QString website() const
    {
        return m_website;
    }

private:
    QString m_name, m_email, m_website;
};

// ---------------------------------------------------------------------------
// Global environment — QCoreApplication must exist before any QObject is used.
// ---------------------------------------------------------------------------

class QtAppEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        static char prog[] = "test_qt_qstring_rules";
        static char* argv[] = { prog, nullptr };
        static int argc = 1;
        m_app = std::make_unique<QCoreApplication>(argc, argv);
    }
    void TearDown() override
    {
        m_app.reset();
    }

private:
    std::unique_ptr<QCoreApplication> m_app;
};

::testing::Environment* const kQtEnv = ::testing::AddGlobalTestEnvironment(new QtAppEnvironment);

// ---------------------------------------------------------------------------
// string_rules::empty
// An empty string passes; any non-empty string (including whitespace) fails.
// ---------------------------------------------------------------------------

TEST(QStringRulesEmptyTest, AcceptsEmptyQString)
{
    EXPECT_TRUE(vd::qt::string_rules::empty()(QString()));
    EXPECT_TRUE(vd::qt::string_rules::empty()(QString("")));
}

TEST(QStringRulesEmptyTest, AcceptsEmptyQStringView)
{
    QString s;
    EXPECT_TRUE(vd::qt::string_rules::empty()(QStringView(s)));
}

TEST(QStringRulesEmptyTest, RejectsNonEmpty)
{
    EXPECT_FALSE(vd::qt::string_rules::empty()(QString("x")));
    EXPECT_FALSE(vd::qt::string_rules::empty()(QString("hello world")));
}

TEST(QStringRulesEmptyTest, RejectsWhitespaceOnly)
{
    EXPECT_FALSE(vd::qt::string_rules::empty()(QString(" ")));
    EXPECT_FALSE(vd::qt::string_rules::empty()(QString("\t\n")));
}

// ---------------------------------------------------------------------------
// string_rules::non_empty
// Non-empty strings (including whitespace) pass; only "" fails.
// ---------------------------------------------------------------------------

TEST(QStringRulesNonEmptyTest, AcceptsNonEmpty)
{
    EXPECT_TRUE(vd::qt::string_rules::non_empty()(QString("x")));
    EXPECT_TRUE(vd::qt::string_rules::non_empty()(QString("hello world")));
}

TEST(QStringRulesNonEmptyTest, AcceptsWhitespaceOnly)
{
    EXPECT_TRUE(vd::qt::string_rules::non_empty()(QString(" ")));
    EXPECT_TRUE(vd::qt::string_rules::non_empty()(QString("\t")));
}

TEST(QStringRulesNonEmptyTest, RejectsEmptyQString)
{
    EXPECT_FALSE(vd::qt::string_rules::non_empty()(QString()));
}

TEST(QStringRulesNonEmptyTest, RejectsEmptyQStringView)
{
    QString s;
    EXPECT_FALSE(vd::qt::string_rules::non_empty()(QStringView(s)));
}

// ---------------------------------------------------------------------------
// string_rules::empty_or_whitespace
// Passes for "" or strings consisting entirely of whitespace.
// Uses QChar::isSpace() which covers Unicode whitespace categories.
// ---------------------------------------------------------------------------

TEST(QStringRulesEmptyOrWhitespaceTest, AcceptsEmptyQString)
{
    EXPECT_TRUE(vd::qt::string_rules::empty_or_whitespace()(QString()));
}

TEST(QStringRulesEmptyOrWhitespaceTest, AcceptsAllAsciiWhitespace)
{
    EXPECT_TRUE(vd::qt::string_rules::empty_or_whitespace()(QString("   ")));
    EXPECT_TRUE(vd::qt::string_rules::empty_or_whitespace()(QString("\t\n\r\f\v")));
    EXPECT_TRUE(vd::qt::string_rules::empty_or_whitespace()(QString("  \t  ")));
}

TEST(QStringRulesEmptyOrWhitespaceTest, RejectsStringWithContent)
{
    EXPECT_FALSE(vd::qt::string_rules::empty_or_whitespace()(QString("x")));
    EXPECT_FALSE(vd::qt::string_rules::empty_or_whitespace()(QString("  x  ")));
    EXPECT_FALSE(vd::qt::string_rules::empty_or_whitespace()(QString("hello")));
}

// ---------------------------------------------------------------------------
// string_rules::email_like
// Heuristic: \S+@\S+\.\S+
// ---------------------------------------------------------------------------

TEST(QStringRulesEmailLikeTest, AcceptsTypicalEmail)
{
    EXPECT_TRUE(vd::qt::string_rules::email_like()(QString("user@example.com")));
    EXPECT_TRUE(vd::qt::string_rules::email_like()(QString("a@b.c")));
    EXPECT_TRUE(vd::qt::string_rules::email_like()(QString("name.surname@company.org")));
}

TEST(QStringRulesEmailLikeTest, RejectsMissingAt)
{
    EXPECT_FALSE(vd::qt::string_rules::email_like()(QString("notanemail")));
    EXPECT_FALSE(vd::qt::string_rules::email_like()(QString("nodot.com")));
}

TEST(QStringRulesEmailLikeTest, RejectsMissingDomain)
{
    EXPECT_FALSE(vd::qt::string_rules::email_like()(QString("user@")));
    EXPECT_FALSE(vd::qt::string_rules::email_like()(QString("user@nodot")));
}

TEST(QStringRulesEmailLikeTest, RejectsEmpty)
{
    EXPECT_FALSE(vd::qt::string_rules::email_like()(QString()));
}

TEST(QStringRulesEmailLikeTest, WorksWithQStringView)
{
    QString data = "user@example.com";
    EXPECT_TRUE(vd::qt::string_rules::email_like()(QStringView(data)));
}

// ---------------------------------------------------------------------------
// string_rules::uri_like
// Heuristic: \w+://\S+
// ---------------------------------------------------------------------------

TEST(QStringRulesUriLikeTest, AcceptsCommonSchemes)
{
    EXPECT_TRUE(vd::qt::string_rules::uri_like()(QString("https://example.com")));
    EXPECT_TRUE(vd::qt::string_rules::uri_like()(QString("http://localhost")));
    EXPECT_TRUE(vd::qt::string_rules::uri_like()(QString("ftp://files.example.com/path")));
}

TEST(QStringRulesUriLikeTest, RejectsMissingScheme)
{
    EXPECT_FALSE(vd::qt::string_rules::uri_like()(QString("example.com")));
    EXPECT_FALSE(vd::qt::string_rules::uri_like()(QString("//example.com")));
}

TEST(QStringRulesUriLikeTest, RejectsMissingSeparator)
{
    EXPECT_FALSE(vd::qt::string_rules::uri_like()(QString("httpexample.com")));
}

TEST(QStringRulesUriLikeTest, RejectsEmpty)
{
    EXPECT_FALSE(vd::qt::string_rules::uri_like()(QString()));
}

TEST(QStringRulesUriLikeTest, WorksWithQStringView)
{
    QString data = "https://example.com";
    EXPECT_TRUE(vd::qt::string_rules::uri_like()(QStringView(data)));
}

// ---------------------------------------------------------------------------
// string_rules::regex
// Runtime PCRE2 pattern via QRegularExpression; full-string match semantics.
// ---------------------------------------------------------------------------

TEST(QStringRulesRegexTest, MatchesLiteralPattern)
{
    auto checker = vd::qt::string_rules::regex("hello");
    EXPECT_TRUE(checker(QString("hello")));
    EXPECT_FALSE(checker(QString("world")));
    EXPECT_FALSE(checker(QString("hello world"))); // full-string match only
}

TEST(QStringRulesRegexTest, MatchesDigitPattern)
{
    auto checker = vd::qt::string_rules::regex(R"(\d+)");
    EXPECT_TRUE(checker(QString("123")));
    EXPECT_TRUE(checker(QString("0")));
    EXPECT_FALSE(checker(QString("abc")));
    EXPECT_FALSE(checker(QString("12x")));
}

TEST(QStringRulesRegexTest, MatchesEmptyPattern)
{
    // An empty regex matches only the empty string under full-match semantics.
    auto checker = vd::qt::string_rules::regex("");
    EXPECT_TRUE(checker(QString()));
    EXPECT_FALSE(checker(QString("x")));
}

TEST(QStringRulesRegexTest, WorksWithQStringView)
{
    auto checker = vd::qt::string_rules::regex(R"(\w+)");
    QString data = "hello";
    EXPECT_TRUE(checker(QStringView(data)));
}

TEST(QStringRulesRegexDeathTest, InvalidPatternAborts)
{
    // An invalid regex pattern must trigger vd::require and abort.
    EXPECT_DEATH(vd::qt::string_rules::regex("[invalid"), "");
}

// ---------------------------------------------------------------------------
// string_rules::max_length
// Passes when QStringView::size() (UTF-16 code units) is <= max_len.
// ---------------------------------------------------------------------------

TEST(QStringRulesMaxLengthTest, AcceptsShorterOrEqualLength)
{
    auto checker = vd::qt::string_rules::max_length(5);
    EXPECT_TRUE(checker(QString("")));
    EXPECT_TRUE(checker(QString("abc")));
    EXPECT_TRUE(checker(QString("abcde")));
}

TEST(QStringRulesMaxLengthTest, RejectsLongerLength)
{
    auto checker = vd::qt::string_rules::max_length(5);
    EXPECT_FALSE(checker(QString("abcdef")));
    EXPECT_FALSE(checker(QString("a very long string")));
}

TEST(QStringRulesMaxLengthTest, WorksWithQStringView)
{
    auto checker = vd::qt::string_rules::max_length(5);
    QString data = "abcdef";
    EXPECT_FALSE(checker(QStringView(data)));
}

TEST(QStringRulesMaxLengthDeathTest, ZeroMaxLenAborts)
{
    EXPECT_THROW(vd::qt::string_rules::max_length(0), vd::assertion_exception);
}

// ---------------------------------------------------------------------------
// string_rules::min_length
// Passes when QStringView::size() (UTF-16 code units) is >= min_len.
// ---------------------------------------------------------------------------

TEST(QStringRulesMinLengthTest, AcceptsLongerOrEqualLength)
{
    auto checker = vd::qt::string_rules::min_length(3);
    EXPECT_TRUE(checker(QString("abc")));
    EXPECT_TRUE(checker(QString("abcdef")));
}

TEST(QStringRulesMinLengthTest, RejectsShorterLength)
{
    auto checker = vd::qt::string_rules::min_length(3);
    EXPECT_FALSE(checker(QString("ab")));
    EXPECT_FALSE(checker(QString("")));
}

TEST(QStringRulesMinLengthTest, WorksWithQStringView)
{
    auto checker = vd::qt::string_rules::min_length(3);
    QString data = "ab";
    EXPECT_FALSE(checker(QStringView(data)));
}

TEST(QStringRulesMinLengthDeathTest, ZeroMinLenAborts)
{
    EXPECT_THROW(vd::qt::string_rules::min_length(0), vd::assertion_exception);
}

// ---------------------------------------------------------------------------
// string_rules::length_in_between
// Passes when QStringView::size() is within [min_len, max_len].
// ---------------------------------------------------------------------------

TEST(QStringRulesLengthInBetweenTest, AcceptsLengthWithinRange)
{
    auto checker = vd::qt::string_rules::length_in_between(3, 6);
    EXPECT_TRUE(checker(QString("abc")));
    EXPECT_TRUE(checker(QString("abcd")));
    EXPECT_TRUE(checker(QString("abcdef")));
}

TEST(QStringRulesLengthInBetweenTest, RejectsLengthOutsideRange)
{
    auto checker = vd::qt::string_rules::length_in_between(3, 6);
    EXPECT_FALSE(checker(QString("ab")));
    EXPECT_FALSE(checker(QString("abcdefg")));
}

TEST(QStringRulesLengthInBetweenTest, WorksWithQStringView)
{
    auto checker = vd::qt::string_rules::length_in_between(3, 6);
    QString data = "ab";
    EXPECT_FALSE(checker(QStringView(data)));
}

TEST(QStringRulesLengthInBetweenDeathTest, InvalidRangeAborts)
{
    EXPECT_THROW(vd::qt::string_rules::length_in_between(0, 5), vd::assertion_exception);
    EXPECT_THROW(vd::qt::string_rules::length_in_between(5, 0), vd::assertion_exception);
    EXPECT_THROW(vd::qt::string_rules::length_in_between(10, 5), vd::assertion_exception);
}

// ---------------------------------------------------------------------------
// Integration: vd::member with Qt string rules on a plain QString struct.
// qstring_match::operator() takes QStringView; const QString& implicitly
// converts, so value_checker<qstring_match<...>, QString> is satisfied.
// ---------------------------------------------------------------------------

TEST(QStringRulesMemberTest, MemberNonEmptyValidatesName)
{
    auto model = vd::basic_model<QStringProfile>().with(vd::member(&QStringProfile::name, vd::qt::string_rules::non_empty()));

    EXPECT_TRUE(model.check(QStringProfile { "Alice", "", "" }));
    EXPECT_FALSE(model.check(QStringProfile { "", "", "" }));
}

TEST(QStringRulesMemberTest, MemberEmailLikeValidatesEmail)
{
    auto model = vd::basic_model<QStringProfile>().with(vd::member(&QStringProfile::email, vd::qt::string_rules::email_like()));

    EXPECT_TRUE(model.check(QStringProfile { "x", "user@example.com", "" }));
    EXPECT_FALSE(model.check(QStringProfile { "x", "notanemail", "" }));
}

TEST(QStringRulesMemberTest, MemberUriLikeValidatesWebsite)
{
    auto model = vd::basic_model<QStringProfile>().with(vd::member(&QStringProfile::website, vd::qt::string_rules::uri_like()));

    EXPECT_TRUE(model.check(QStringProfile { "", "", "https://example.com" }));
    EXPECT_FALSE(model.check(QStringProfile { "", "", "not-a-uri" }));
}

TEST(QStringRulesMemberTest, RegexCheckerWorksWithMember)
{
    auto model = vd::basic_model<QStringProfile>().with(vd::member(&QStringProfile::name, vd::qt::string_rules::regex(R"(\w+)")));

    EXPECT_TRUE(model.check(QStringProfile { "Alice123", "", "" }));
    EXPECT_FALSE(model.check(QStringProfile { "Alice 123", "", "" })); // space not allowed
    EXPECT_FALSE(model.check(QStringProfile { "", "", "" }));          // empty fails \w+
}

TEST(QStringRulesMemberTest, MultipleRulesAllMustPass)
{
    auto model = vd::basic_model<QStringProfile>()
                     .with(vd::member(&QStringProfile::name, vd::qt::string_rules::non_empty()))
                     .with(vd::member(&QStringProfile::email, vd::qt::string_rules::email_like()));

    EXPECT_TRUE(model.check(QStringProfile { "Alice", "a@b.com", "" }));
    EXPECT_FALSE(model.check(QStringProfile { "", "a@b.com", "" }));  // name empty
    EXPECT_FALSE(model.check(QStringProfile { "Alice", "bad", "" })); // email invalid
    EXPECT_FALSE(model.check(QStringProfile { "", "bad", "" }));      // both fail
}

// ---------------------------------------------------------------------------
// Integration: vd::qt::qt_property + lambda delegating to qstring_match.
// qt_property extracts PropT via first_arg_of, which requires a single
// operator(). Wrapping in a lambda bridges the QStringView checker with the
// QString property value stored in QVariant.
// ---------------------------------------------------------------------------

TEST(QStringRulesQtPropertyTest, NonEmptyPropertyViaLambda)
{
    auto checker = vd::qt::string_rules::non_empty();
    auto rule = vd::qt::qt_property<StringQObject>("name", [checker](const QString& s) {
        return checker(s);
    });

    StringQObject valid("Alice", "", "");
    StringQObject invalid("", "", "");
    EXPECT_TRUE(rule(valid));
    EXPECT_FALSE(rule(invalid));
}

TEST(QStringRulesQtPropertyTest, EmailLikePropertyViaLambda)
{
    auto checker = vd::qt::string_rules::email_like();
    auto rule = vd::qt::qt_property<StringQObject>("email", [checker](const QString& s) {
        return checker(s);
    });

    StringQObject valid("", "user@example.com", "");
    StringQObject invalid("", "notanemail", "");
    EXPECT_TRUE(rule(valid));
    EXPECT_FALSE(rule(invalid));
}

TEST(QStringRulesQtPropertyTest, UriLikePropertyViaLambda)
{
    auto checker = vd::qt::string_rules::uri_like();
    auto rule = vd::qt::qt_property<StringQObject>("website", [checker](const QString& s) {
        return checker(s);
    });

    StringQObject valid("", "", "https://example.com");
    StringQObject invalid("", "", "example.com");
    EXPECT_TRUE(rule(valid));
    EXPECT_FALSE(rule(invalid));
}

TEST(QStringRulesQtPropertyTest, RegexPropertyViaLambda)
{
    auto checker = vd::qt::string_rules::regex(R"(\w+)");
    auto rule = vd::qt::qt_property<StringQObject>("name", [checker](const QString& s) {
        return checker(s);
    });

    StringQObject valid("Alice123", "", "");
    StringQObject invalid("Alice 123", "", "");
    EXPECT_TRUE(rule(valid));
    EXPECT_FALSE(rule(invalid));
}

TEST(QStringRulesQtPropertyTest, ModelCombiningMultipleStringPropertyRules)
{
    auto name_checker = vd::qt::string_rules::non_empty();
    auto email_checker = vd::qt::string_rules::email_like();

    auto model = vd::basic_model<StringQObject>()
                     .with(vd::qt::qt_property<StringQObject>("name",
                         [name_checker](const QString& s) {
                             return name_checker(s);
                         }))
                     .with(vd::qt::qt_property<StringQObject>("email", [email_checker](const QString& s) {
                         return email_checker(s);
                     }));

    StringQObject valid("Alice", "alice@example.com", "");
    EXPECT_TRUE(model.check(valid));

    StringQObject bad_name("", "alice@example.com", "");
    EXPECT_FALSE(model.check(bad_name));

    StringQObject bad_email("Alice", "notanemail", "");
    EXPECT_FALSE(model.check(bad_email));
}

// moc needed for StringQObject (Q_OBJECT defined in this translation unit)
#include "test_qt_qstring_rules.moc"
