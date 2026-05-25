#include <gtest/gtest.h>

#include <memory>

#include <QCoreApplication>
#include <QObject>
#include <QString>

#include "ext/qt/vd_qtbase.hxx"
#include "models/vd_basic_model.hxx"
#include "models/vd_numeric.hxx"
#include "models/vd_rule_factory.hxx"

// ---------------------------------------------------------------------------
// Test fixture — QObject with Q_PROPERTYs of several value types
// ---------------------------------------------------------------------------

class SampleQObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(double score READ score)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(QString label READ label)

public:
    explicit SampleQObject(double score, int count, const QString& label, QObject* parent = nullptr)
        : QObject(parent), m_score(score), m_count(count), m_label(label)
    {
    }

    double score() const
    {
        return m_score;
    }
    int count() const
    {
        return m_count;
    }
    QString label() const
    {
        return m_label;
    }

private:
    double m_score;
    int m_count;
    QString m_label;
};

// ---------------------------------------------------------------------------
// Global test environment — QCoreApplication must exist before QObject is used.
// QCoreApplication can only be created once per process; the environment
// owns it for the whole GTest run.
// ---------------------------------------------------------------------------

class QtAppEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        static char prog[] = "test_qt_property";
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
// vd::qt::qt_property — double property
// ---------------------------------------------------------------------------

TEST(QtPropertyTest, AcceptsDoublePropertyInRange)
{
    auto rule = vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::inclusive(0.0, 100.0));

    SampleQObject obj(50.0, 0, "");
    EXPECT_TRUE(rule(obj));
}

TEST(QtPropertyTest, RejectsDoublePropertyOutOfRange)
{
    auto rule = vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::inclusive(0.0, 100.0));

    SampleQObject obj(150.0, 0, "");
    EXPECT_FALSE(rule(obj));
}

TEST(QtPropertyTest, AcceptsDoublePropertyAtInclusiveBoundaries)
{
    auto rule = vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::inclusive(0.0, 100.0));

    SampleQObject lo(0.0, 0, "");
    SampleQObject hi(100.0, 0, "");
    EXPECT_TRUE(rule(lo));
    EXPECT_TRUE(rule(hi));
}

TEST(QtPropertyTest, GreaterThanRejectsEqualValue)
{
    auto rule = vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::greater_than(10.0));

    SampleQObject eq(10.0, 0, "");
    SampleQObject above(10.0 + 1e-9, 0, "");
    EXPECT_FALSE(rule(eq));
    EXPECT_TRUE(rule(above));
}

TEST(QtPropertyTest, LessThanRejectsEqualValue)
{
    auto rule = vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::less_than(50.0));

    SampleQObject eq(50.0, 0, "");
    SampleQObject below(49.9, 0, "");
    EXPECT_FALSE(rule(eq));
    EXPECT_TRUE(rule(below));
}

// ---------------------------------------------------------------------------
// vd::qt::qt_property — int property
// ---------------------------------------------------------------------------

TEST(QtPropertyTest, AcceptsIntPropertyInRange)
{
    auto rule = vd::qt::qt_property<SampleQObject>("count", vd::int_bounds::inclusive(1, 99));

    SampleQObject obj(0.0, 42, "");
    EXPECT_TRUE(rule(obj));
}

TEST(QtPropertyTest, RejectsIntPropertyBelowMin)
{
    auto rule = vd::qt::qt_property<SampleQObject>("count", vd::int_bounds::inclusive(1, 99));

    SampleQObject obj(0.0, 0, "");
    EXPECT_FALSE(rule(obj));
}

TEST(QtPropertyTest, RejectsIntPropertyAboveMax)
{
    auto rule = vd::qt::qt_property<SampleQObject>("count", vd::int_bounds::inclusive(1, 99));

    SampleQObject obj(0.0, 100, "");
    EXPECT_FALSE(rule(obj));
}

TEST(QtPropertyTest, IntGreaterThanExcludesBoundary)
{
    // exclusive lower bound: count must be strictly > 5
    auto rule = vd::qt::qt_property<SampleQObject>("count", vd::int_bounds::greater_than(5));

    SampleQObject eq(0.0, 5, "");
    SampleQObject above(0.0, 6, "");
    EXPECT_FALSE(rule(eq));
    EXPECT_TRUE(rule(above));
}

// ---------------------------------------------------------------------------
// vd::qt::qt_property — QString property with lambda checker
// ---------------------------------------------------------------------------

TEST(QtPropertyTest, AcceptsNonEmptyStringProperty)
{
    auto rule = vd::qt::qt_property<SampleQObject>("label", [](const QString& s) {
        return !s.isEmpty();
    });

    SampleQObject obj(0.0, 0, "hello");
    EXPECT_TRUE(rule(obj));
}

TEST(QtPropertyTest, RejectsEmptyStringProperty)
{
    auto rule = vd::qt::qt_property<SampleQObject>("label", [](const QString& s) {
        return !s.isEmpty();
    });

    SampleQObject obj(0.0, 0, "");
    EXPECT_FALSE(rule(obj));
}

TEST(QtPropertyTest, StringLengthCheckerWorks)
{
    auto rule = vd::qt::qt_property<SampleQObject>("label", [](const QString& s) {
        return s.length() <= 10;
    });

    SampleQObject ok(0.0, 0, "short");
    SampleQObject too_long(0.0, 0, "this is way too long for the limit");
    EXPECT_TRUE(rule(ok));
    EXPECT_FALSE(rule(too_long));
}

// ---------------------------------------------------------------------------
// vd::qt::qt_property — edge cases
// ---------------------------------------------------------------------------

TEST(QtPropertyTest, RejectsNonExistentPropertyName)
{
    // QObject::property() returns an invalid QVariant for unknown names.
    auto rule = vd::qt::qt_property<SampleQObject>("nonexistent_xyz", vd::double_bounds::unbounded());

    SampleQObject obj(42.0, 0, "");
    EXPECT_FALSE(rule(obj));
}

// ---------------------------------------------------------------------------
// vd::qt::qt_property — integration with basic_model
// ---------------------------------------------------------------------------

TEST(QtPropertyTest, UsableWithBasicModel)
{
    auto model = vd::basic_model<SampleQObject>()
                     .with(vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::inclusive(0.0, 100.0)))
                     .with(vd::qt::qt_property<SampleQObject>("count", vd::int_bounds::inclusive(1, 10)));

    SampleQObject valid(50.0, 5, "");
    EXPECT_TRUE(model.is_valid(valid));

    SampleQObject bad_score(200.0, 5, "");
    EXPECT_FALSE(model.is_valid(bad_score));

    SampleQObject bad_count(50.0, 0, "");
    EXPECT_FALSE(model.is_valid(bad_count));
}

TEST(QtPropertyTest, InitializerListWithMultiplePropertyRules)
{
    // Both rules must pass: score > 0 AND label not empty.
    auto model = vd::basic_model<SampleQObject>().with({
        vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::greater_than(0.0)),
        vd::qt::qt_property<SampleQObject>("label",
            [](const QString& s) {
                return !s.isEmpty();
            }),
    });

    SampleQObject valid(1.0, 0, "ok");
    EXPECT_TRUE(model.is_valid(valid));

    SampleQObject bad_score(0.0, 0, "ok");
    EXPECT_FALSE(model.is_valid(bad_score));

    SampleQObject bad_label(1.0, 0, "");
    EXPECT_FALSE(model.is_valid(bad_label));
}

TEST(QtPropertyTest, MixedQtPropertyAndMemberRulesInModel)
{
    // qt_property and regular vd::predicate can coexist in one model.
    auto model = vd::basic_model<SampleQObject>()
                     .with(vd::qt::qt_property<SampleQObject>("score", vd::double_bounds::inclusive(0.0, 100.0)))
                     .with(vd::predicate([](const SampleQObject& obj) {
                         return obj.count() >= 0;
                     }));

    SampleQObject valid(50.0, 3, "");
    EXPECT_TRUE(model.is_valid(valid));

    SampleQObject bad_score(200.0, 3, "");
    EXPECT_FALSE(model.is_valid(bad_score));
}

// The moc file is needed because SampleQObject (with Q_OBJECT) is defined
// in this source file, not in a header processed by AUTOMOC separately.
#include "test_qt_property.moc"
