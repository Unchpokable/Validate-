#include <gtest/gtest.h>
#include <memory>
#include <QCoreApplication>
#include <QObject>
#include <QString>
#include "ext/qt/vd_qtbase.hxx"
#include "models/vd_basic_model.hxx"
#include "models/vd_rule_factory.hxx"

class Obj : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
public:
    explicit Obj(const QString& n, QObject* p=nullptr) : QObject(p), m_name(n) {}
    QString name() const { return m_name; }
private: QString m_name;
};

class Env : public ::testing::Environment {
public:
    void SetUp() override {
        static char prog[] = "t"; static char* argv[] = { prog, nullptr }; static int argc = 1;
        m_app = std::make_unique<QCoreApplication>(argc, argv);
    }
    void TearDown() override { m_app.reset(); }
private: std::unique_ptr<QCoreApplication> m_app;
};
::testing::Environment* const kE = ::testing::AddGlobalTestEnvironment(new Env);

static vd::basic_model<Obj> make_model() {
    return vd::basic_model<Obj>{}.with(vd::qt::qt_property<Obj>("name", [](const QString& s){ return !s.isEmpty(); }));
}

TEST(T, DirectCall) {
    auto m = make_model();
    Obj obj("test");
    EXPECT_TRUE(m.is_valid(obj));
}

TEST(T, CopyThenDirectCall) {
    auto m = make_model();
    auto m2 = m; // copy
    Obj obj("test");
    EXPECT_TRUE(m2.is_valid(obj)); // call on copy
}

TEST(T, CopyInLambdaNoCall) {
    auto m = make_model();
    auto lam = [m]() { return 42; }; // copy, but never call m.is_valid
    EXPECT_EQ(lam(), 42);
}

TEST(T, CopyInLambdaAndCall) {
    auto m = make_model();
    auto lam = [m]() { Obj o("x"); return m.is_valid(o); };
    EXPECT_TRUE(lam()); // call through lambda
}

#include "diag_test.moc"
