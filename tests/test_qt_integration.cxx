#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QStringView>
#include <QVariant>

#include "assert/vd_assert.hxx"
#include "ext/qt/vd_qtbase.hxx"
#include "models/vd_basic_model.hxx"
#include "models/vd_numeric.hxx"
#include "models/vd_rule_factory.hxx"
#include "vd_core.hxx"

// ---------------------------------------------------------------------------
// QObject hierarchy: CityObject <- AddressObject <- UserAccount
// Three-level nesting for closure / model-capture tests.
// ---------------------------------------------------------------------------

class CityObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString zip  READ zip)
public:
    explicit CityObject(const QString& name, const QString& zip, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_zip(zip) {}
    QString name() const { return m_name; }
    QString zip()  const { return m_zip; }
private:
    QString m_name, m_zip;
};

class AddressObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString street READ street)
public:
    explicit AddressObject(const QString& street, CityObject* city, QObject* parent = nullptr)
        : QObject(parent), m_street(street), m_city(city) {}
    QString     street() const { return m_street; }
    CityObject* city()   const { return m_city; }
private:
    QString     m_street;
    CityObject* m_city = nullptr;
};

class UserAccount : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString username READ username)
    Q_PROPERTY(QString email    READ email)
    Q_PROPERTY(int     age      READ age)
public:
    explicit UserAccount(const QString& username, const QString& email,
                         int age, AddressObject* address, QObject* parent = nullptr)
        : QObject(parent), m_username(username), m_email(email),
          m_age(age), m_address(address) {}
    QString        username() const { return m_username; }
    QString        email()    const { return m_email; }
    int            age()      const { return m_age; }
    AddressObject* address()  const { return m_address; }
private:
    QString        m_username, m_email;
    int            m_age = 0;
    AddressObject* m_address = nullptr;
};

// Mutable QObject for bound-model mutation tests.
class MutableSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString token  READ token  WRITE setToken)
    Q_PROPERTY(int     ttl    READ ttl    WRITE setTtl)
    Q_PROPERTY(QString userId READ userId WRITE setUserId)
public:
    explicit MutableSession(QObject* parent = nullptr) : QObject(parent) {}
    QString token()  const { return m_token; }
    int     ttl()    const { return m_ttl; }
    QString userId() const { return m_userId; }
    void setToken (const QString& t) { m_token  = t; }
    void setTtl   (int t)            { m_ttl    = t; }
    void setUserId(const QString& u) { m_userId = u; }
private:
    QString m_token, m_userId;
    int     m_ttl = 0;
};

// QObject representing an API request — all fields immutable after construction.
class ApiRequestObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString endpoint   READ endpoint)
    Q_PROPERTY(QString method     READ method)
    Q_PROPERTY(int     timeout_ms READ timeoutMs)
    Q_PROPERTY(QString auth_token READ authToken)
    Q_PROPERTY(double  rate_limit READ rateLimit)
public:
    explicit ApiRequestObject(const QString& endpoint, const QString& method,
                               int timeout_ms, const QString& auth_token, double rate_limit,
                               QObject* parent = nullptr)
        : QObject(parent), m_endpoint(endpoint), m_method(method),
          m_timeout_ms(timeout_ms), m_auth_token(auth_token), m_rate_limit(rate_limit) {}
    QString endpoint()   const { return m_endpoint; }
    QString method()     const { return m_method; }
    int     timeoutMs()  const { return m_timeout_ms; }
    QString authToken()  const { return m_auth_token; }
    double  rateLimit()  const { return m_rate_limit; }
private:
    QString m_endpoint, m_method, m_auth_token;
    int     m_timeout_ms = 0;
    double  m_rate_limit  = 0.0;
};

// ---------------------------------------------------------------------------
// Global QCoreApplication environment — one per process.
// ---------------------------------------------------------------------------

class QtIntegrationAppEnv : public ::testing::Environment {
public:
    void SetUp() override {
        static char  prog[]  = "test_qt_integration";
        static char* argv[]  = { prog, nullptr };
        static int   argc    = 1;
        m_app = std::make_unique<QCoreApplication>(argc, argv);
    }
    void TearDown() override { m_app.reset(); }
private:
    std::unique_ptr<QCoreApplication> m_app;
};

::testing::Environment* const kQtEnv =
    ::testing::AddGlobalTestEnvironment(new QtIntegrationAppEnv);

// ---------------------------------------------------------------------------
// Model factory helpers
//
// NOTE: qt_property + string-rule checkers (qstring_match / qregex_checker)
// require lambda wrappers so that first_arg_of deduces PropT = QString
// rather than QStringView — QVariant::canConvert<QStringView>() is false.
// ---------------------------------------------------------------------------

static vd::basic_model<CityObject> make_city_model()
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");
    return vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }));
}

// Takes city_model by value so the caller controls copy vs. move at the call site.
static vd::basic_model<AddressObject>
make_address_model(vd::basic_model<CityObject> city_model)
{
    return vd::basic_model<AddressObject>{}
        .with(vd::qt::qt_property<AddressObject>("street",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::predicate([cm = std::move(city_model)](const AddressObject& a) {
            auto* c = a.city();
            return c != nullptr && cm.is_valid(*c);
        }));
}

static vd::basic_model<UserAccount>
make_user_model(vd::basic_model<AddressObject> addr_model)
{
    auto email_checker = vd::qt::string_rules::email_like();
    return vd::basic_model<UserAccount>{}
        .with(vd::qt::qt_property<UserAccount>("username",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<UserAccount>("email",
            [email_checker](const QString& s) { return email_checker(s); }))
        .with(vd::qt::qt_property<UserAccount>("age",
            [](int a) { return a >= 18 && a <= 120; }))
        .with(vd::predicate([am = std::move(addr_model)](const UserAccount& u) {
            auto* a = u.address();
            return a != nullptr && am.is_valid(*a);
        }));
}

static vd::basic_model<MutableSession> make_session_model()
{
    return vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) { return t > 0; }))
        .with(vd::qt::qt_property<MutableSession>("userId",
            [](const QString& s) { return !s.isEmpty(); }));
}

// ---------------------------------------------------------------------------
// Group 1: QtNestedObjectValidation
// Validates the full UserAccount -> AddressObject -> CityObject chain.
// Each model level is captured by value inside the outer rule's closure.
// ---------------------------------------------------------------------------

TEST(QtNestedObjectValidation, ValidCityPassesCityModel)
{
    auto model = make_city_model();
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(model.is_valid(city));
}

TEST(QtNestedObjectValidation, EmptyCityNameFailsCityModel)
{
    auto model = make_city_model();
    CityObject city("", "10115");
    EXPECT_FALSE(model.is_valid(city));
}

TEST(QtNestedObjectValidation, InvalidZipFailsCityModel)
{
    auto model = make_city_model();
    CityObject alpha ("Berlin", "ABC12"); // not all digits
    CityObject short4("Berlin", "1011");  // 4 digits
    CityObject long6 ("Berlin", "101150"); // 6 digits
    EXPECT_FALSE(model.is_valid(alpha));
    EXPECT_FALSE(model.is_valid(short4));
    EXPECT_FALSE(model.is_valid(long6));
}

TEST(QtNestedObjectValidation, ValidAddressWithValidCityPasses)
{
    CityObject    city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    EXPECT_TRUE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, AddressWithNullCityFails)
{
    AddressObject addr("Unter den Linden 1", nullptr);
    EXPECT_FALSE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, AddressWithInvalidCityFails)
{
    CityObject    bad_city("", "10115");
    AddressObject addr("Unter den Linden 1", &bad_city);
    EXPECT_FALSE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, EmptyStreetFailsAddressModel)
{
    CityObject    city("Berlin", "10115");
    AddressObject addr("", &city);
    EXPECT_FALSE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, ValidUserAccountWithFullHierarchyPasses)
{
    CityObject    city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount   user("alice", "alice@example.com", 30, &addr);
    EXPECT_TRUE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithNullAddressFails)
{
    UserAccount user("alice", "alice@example.com", 30, nullptr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithInvalidEmailFails)
{
    CityObject    city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount   user("alice", "not-an-email", 30, &addr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountUnderageFailsAgeRule)
{
    CityObject    city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount   user("alice", "alice@example.com", 16, &addr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithBadZipDeepInChainFails)
{
    CityObject    bad_city("Hamburg", "ABCDE"); // zip fails at the deepest level
    AddressObject addr("Jungfernstieg 1", &bad_city);
    UserAccount   user("bob", "bob@mail.de", 25, &addr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

// ---------------------------------------------------------------------------
// Group 2: QtModelClosureCopyCapture
// Verify that a basic_model<> captured by copy in a lambda is truly
// independent from the original after the capture point.
// ---------------------------------------------------------------------------

TEST(QtModelClosureCopyCapture, CapturedCityModelValidatesCorrectly)
{
    auto city_model = make_city_model();

    auto addr_rule = vd::predicate([city_model](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model.is_valid(*c);
    });

    CityObject    good_city("Paris", "75001");
    AddressObject good_addr("Rue de Rivoli", &good_city);
    EXPECT_TRUE(addr_rule(good_addr));

    CityObject    bad_city("", "75001"); // empty name fails
    AddressObject bad_addr("Rue de Rivoli", &bad_city);
    EXPECT_FALSE(addr_rule(bad_addr));
}

TEST(QtModelClosureCopyCapture, AddingRuleToOriginalAfterCaptureDoesNotAffectClosure)
{
    auto city_model = make_city_model();

    // Capture a copy — the snapshot has only the two base rules (name + zip).
    auto addr_rule = vd::predicate([snapshot = city_model](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && snapshot.is_valid(*c);
    });

    // Add an extra rule to the original that rejects everything except one name.
    city_model.add_rule(vd::rule<CityObject>([](const CityObject& c) {
        return c.name() == "ONLY_THIS_NAME_PASSES";
    }));

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    // Original now rejects "Berlin" due to the extra rule.
    EXPECT_FALSE(city_model.is_valid(city));

    // The closure captured the old copy — "Berlin" still passes name + zip rules.
    EXPECT_TRUE(addr_rule(addr));
}

TEST(QtModelClosureCopyCapture, TwoRulesCaptureIndependentCopies)
{
    // model_strict always fails; model_normal is the standard city model.
    auto city_model_strict = make_city_model();
    city_model_strict.add_rule(
        vd::rule<CityObject>([](const CityObject&) { return false; }));

    auto city_model_normal = make_city_model();

    auto rule_strict = vd::predicate([city_model_strict](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model_strict.is_valid(*c);
    });
    auto rule_normal = vd::predicate([city_model_normal](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model_normal.is_valid(*c);
    });

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street", &city);

    EXPECT_FALSE(rule_strict(addr)); // strict model always fails
    EXPECT_TRUE(rule_normal(addr));  // normal model accepts valid city
}

TEST(QtModelClosureCopyCapture, CapturedModelSurvivesOuterModelCopyAndMove)
{
    auto city_model = make_city_model();

    auto addr_model = vd::basic_model<AddressObject>{}
        .with(vd::predicate([city_model](const AddressObject& a) {
            auto* c = a.city();
            return c != nullptr && city_model.is_valid(*c);
        }));

    auto addr_copy   = addr_model;             // copy — should carry captured city_model
    auto addr_moved  = std::move(addr_copy);   // move — addr_copy is now empty (vacuous)

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    EXPECT_TRUE(addr_model.is_valid(addr));  // original still works
    EXPECT_TRUE(addr_moved.is_valid(addr));  // moved model carries the closure intact
}

TEST(QtModelClosureCopyCapture, NestedStringRuleCheckersCapturedInsideClosure)
{
    // Two runtime-constructed string checkers captured inside a model's rule
    // which is itself captured inside another model's rule.
    auto name_checker = vd::qt::string_rules::non_empty();
    auto zip_checker  = vd::qt::string_rules::regex(R"(\d{5})");

    auto city_model = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [name_checker](const QString& s) { return name_checker(s); }))
        .with(vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }));

    // city_model's rules each hold a copy of their respective checker.
    // Now capture city_model itself inside an addr predicate.
    auto addr_rule = vd::predicate([city_model](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model.is_valid(*c);
    });

    CityObject    valid_city("Vienna", "01010");
    AddressObject valid_addr("Ringstrasse 1", &valid_city);
    EXPECT_TRUE(addr_rule(valid_addr));

    CityObject    bad_city("Vienna", "X1010");
    AddressObject bad_addr("Ringstrasse 1", &bad_city);
    EXPECT_FALSE(addr_rule(bad_addr));
}

// ---------------------------------------------------------------------------
// Group 3: QtModelMoveSemantics
// Moving a basic_model<> into a closure leaves the source vacuously valid
// (empty rules). The destination closure has full validation capability.
// The rvalue with() overload moves the model forward each step.
// ---------------------------------------------------------------------------

TEST(QtModelMoveSemantics, MovedFromModelIsVacuouslyValid)
{
    auto city_model = make_city_model();

    // Move city_model into the lambda — source loses all its rules.
    auto rule = vd::predicate([cm = std::move(city_model)](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && cm.is_valid(*c);
    });
    (void)rule;

    // Moved-from model has empty m_rules vector → is_valid() returns true
    // for any non-null object (vacuous truth over empty rule set).
    CityObject all_bad("", "XXXXX");
    EXPECT_TRUE(city_model.is_valid(all_bad));
}

TEST(QtModelMoveSemantics, ClosureReceivingMovedModelEnforcesAllRules)
{
    auto city_model = make_city_model();

    auto rule = vd::predicate([cm = std::move(city_model)](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && cm.is_valid(*c);
    });

    CityObject    good("Berlin", "10115");
    AddressObject good_addr("Street", &good);
    EXPECT_TRUE(rule(good_addr));

    CityObject    bad("", "10115"); // empty name fails
    AddressObject bad_addr("Street", &bad);
    EXPECT_FALSE(rule(bad_addr));
}

TEST(QtModelMoveSemantics, RvalueWithChainBuildsCorrectModel)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    // Each with() on an rvalue returns a new model (std::move(*this) internally).
    auto model = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }));

    CityObject valid("Munich", "80331");
    CityObject bad_zip("Munich", "8033");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(bad_zip));
}

TEST(QtModelMoveSemantics, ModelMovedIntoAnotherModelViaRvalueWith)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto base = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }));

    // Rvalue with() on std::move(base) — base is now moved-from.
    auto extended = std::move(base).with(
        vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }));

    CityObject city("Munich", "80331");
    CityObject bad_zip("Munich", "803");

    EXPECT_TRUE(base.is_valid(city));       // vacuous: base is empty
    EXPECT_TRUE(base.is_valid(bad_zip));    // vacuous: base is empty

    EXPECT_TRUE(extended.is_valid(city));   // both rules present
    EXPECT_FALSE(extended.is_valid(bad_zip));
}

TEST(QtModelMoveSemantics, ThreeLevelChainBuiltWithMovesValidatesCorrectly)
{
    auto city_model = make_city_model();
    auto addr_model = make_address_model(std::move(city_model));
    auto user_model = make_user_model(std::move(addr_model));

    // city_model and addr_model are now empty (moved into closures).
    // user_model holds all three levels inside nested std::function objects.

    CityObject    good_city("Hamburg", "20095");
    AddressObject good_addr("Jungfernstieg 1", &good_city);
    UserAccount   good_user("bob", "bob@mail.de", 25, &good_addr);
    EXPECT_TRUE(user_model.is_valid(good_user));

    CityObject    bad_city("Hamburg", "ABCDE"); // zip fails at the deepest level
    AddressObject bad_addr("Jungfernstieg 1", &bad_city);
    UserAccount   bad_user("bob", "bob@mail.de", 25, &bad_addr);
    EXPECT_FALSE(user_model.is_valid(bad_user));
}

// ---------------------------------------------------------------------------
// Group 4: QtBoundModelMutation
// basic_bound_model stores const T* — mutations to the live QObject
// are visible on the very next call to is_valid() or dead_check().
// ---------------------------------------------------------------------------

TEST(QtBoundModelMutation, BoundModelReflectsTtlMutationImmediately)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok_abc123");
    session.setTtl(3600);
    session.setUserId("user_42");

    auto bound = model.bind(session);
    EXPECT_TRUE(bound.is_valid());

    session.setTtl(-1);   // session expired
    EXPECT_FALSE(bound.is_valid());

    session.setTtl(900);  // refreshed
    EXPECT_TRUE(bound.is_valid());
}

TEST(QtBoundModelMutation, BoundModelReflectsTokenRevocation)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok_valid");
    session.setTtl(3600);
    session.setUserId("user_1");

    auto bound = model.bind(session);
    EXPECT_TRUE(bound.is_valid());

    session.setToken(""); // token revoked
    EXPECT_FALSE(bound.is_valid());
}

TEST(QtBoundModelMutation, BoundModelTracksEachPropertyIndependently)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok");
    session.setTtl(100);
    session.setUserId("u1");

    auto bound = model.bind(session);

    // Break and restore userId
    session.setUserId("");
    EXPECT_FALSE(bound.is_valid());
    session.setUserId("u1");
    EXPECT_TRUE(bound.is_valid());

    // Break and restore token
    session.setToken("");
    EXPECT_FALSE(bound.is_valid());
    session.setToken("tok");
    EXPECT_TRUE(bound.is_valid());

    // Break and restore ttl
    session.setTtl(0);
    EXPECT_FALSE(bound.is_valid());
    session.setTtl(100);
    EXPECT_TRUE(bound.is_valid());
}

TEST(QtBoundModelMutation, TwoBoundInstancesOnSameObjectAreIndependent)
{
    auto model_token = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) { return !s.isEmpty(); }));

    auto model_ttl = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) { return t > 0; }));

    MutableSession session;
    session.setToken("tok");
    session.setTtl(100);
    session.setUserId("u");

    auto bound_token = model_token.bind(session);
    auto bound_ttl   = model_ttl.bind(session);

    session.setToken(""); // violates model_token only
    EXPECT_FALSE(bound_token.is_valid());
    EXPECT_TRUE(bound_ttl.is_valid());

    session.setToken("tok");
    session.setTtl(0); // violates model_ttl only
    EXPECT_TRUE(bound_token.is_valid());
    EXPECT_FALSE(bound_ttl.is_valid());
}

TEST(QtBoundModelMutation, BoundModelWithNestedClosureSeesOuterMutation)
{
    // Outer model captures inner model by value.
    // Bound model stores const T* — re-checks the whole closure chain on each call.
    auto inner_model = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) { return t > 0; }));

    auto outer_model = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::predicate([inner_model](const MutableSession& s) {
            return inner_model.is_valid(s);
        }));

    MutableSession session;
    session.setToken("tok");
    session.setTtl(3600);
    session.setUserId("u");

    auto bound = outer_model.bind(session);
    EXPECT_TRUE(bound.is_valid());

    session.setTtl(-1); // inner model rejects this
    EXPECT_FALSE(bound.is_valid());
}

// ---------------------------------------------------------------------------
// Group 5: QtDeadCheck
// dead_check() throws vd::validation_exception when validation fails.
// ---------------------------------------------------------------------------

TEST(QtDeadCheck, ThrowsOnFailingQtProperty)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken(""); // empty — fails
    session.setTtl(3600);
    session.setUserId("u");

    EXPECT_THROW(model.dead_check(session), vd::validation_exception);
}

TEST(QtDeadCheck, DoesNotThrowWhenAllPropertiesValid)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok_valid");
    session.setTtl(3600);
    session.setUserId("user_1");

    EXPECT_NO_THROW(model.dead_check(session));
}

TEST(QtDeadCheck, BoundModelDeadCheckThrowsAfterMutation)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok");
    session.setTtl(3600);
    session.setUserId("u");

    auto bound = model.bind(session);
    EXPECT_NO_THROW(bound.dead_check());

    session.setTtl(0);
    EXPECT_THROW(bound.dead_check(), vd::validation_exception);
}

TEST(QtDeadCheck, ThrowsWhenNestedModelDeepInChainFails)
{
    CityObject    bad_city("Berlin", "ABCDE"); // bad zip deep in the chain
    AddressObject addr("Street 1", &bad_city);
    UserAccount   user("alice", "alice@example.com", 30, &addr);

    EXPECT_THROW(
        make_user_model(make_address_model(make_city_model())).dead_check(user),
        vd::validation_exception);
}

TEST(QtDeadCheck, ExceptionIsvdValidationException)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("");
    session.setTtl(100);
    session.setUserId("u");

    try {
        model.dead_check(session);
        FAIL() << "Expected vd::validation_exception";
    } catch (const vd::validation_exception& ex) {
        EXPECT_NE(std::string(ex.what()), "");
    } catch (...) {
        FAIL() << "Wrong exception type thrown";
    }
}

// ---------------------------------------------------------------------------
// Group 6: QtModelCompositionWithQtRules
// with(other_model) merges Qt-rule models; the combined model enforces all
// rules from both sources.
// ---------------------------------------------------------------------------

TEST(QtModelCompositionWithQtRules, BaseAndExtensionComposedEnforceAllRules)
{
    auto base = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) { return !s.isEmpty(); }));

    auto extended = vd::basic_model<MutableSession>{}
        .with(base)
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) { return t > 0; }));

    MutableSession s;
    s.setToken("tok");
    s.setTtl(100);
    s.setUserId("u");
    EXPECT_TRUE(extended.is_valid(s));

    s.setToken("");   // violates base rule
    EXPECT_FALSE(extended.is_valid(s));

    s.setToken("tok");
    s.setTtl(0);      // violates extended rule
    EXPECT_FALSE(extended.is_valid(s));
}

TEST(QtModelCompositionWithQtRules, TwoSeparateModelsComposed)
{
    auto model_a = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<MutableSession>("userId",
            [](const QString& s) { return !s.isEmpty(); }));

    auto model_b = vd::basic_model<MutableSession>{}
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) { return t > 0; }));

    auto composed = vd::basic_model<MutableSession>{}.with(model_a).with(model_b);

    MutableSession s;
    s.setToken("tok");
    s.setUserId("u");
    s.setTtl(100);
    EXPECT_TRUE(composed.is_valid(s));

    s.setToken("");  EXPECT_FALSE(composed.is_valid(s));
    s.setToken("tok");
    s.setUserId(""); EXPECT_FALSE(composed.is_valid(s));
    s.setUserId("u");
    s.setTtl(-1);   EXPECT_FALSE(composed.is_valid(s));
}

TEST(QtModelCompositionWithQtRules, RvalueWithMergesModelsCorrectly)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto part_a = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }));

    auto part_b = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }));

    auto full = std::move(part_a).with(part_b);

    CityObject valid("Hamburg", "20095");
    CityObject bad_zip("Hamburg", "2009X");
    EXPECT_TRUE(full.is_valid(valid));
    EXPECT_FALSE(full.is_valid(bad_zip));
}

TEST(QtModelCompositionWithQtRules, EmptyModelComposedWithRuledModel)
{
    auto empty  = vd::basic_model<CityObject>{};
    auto ruled  = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }));

    auto composed = vd::basic_model<CityObject>{}.with(empty).with(ruled);

    CityObject good("Berlin", "10115");
    CityObject bad ("", "10115");
    EXPECT_TRUE(composed.is_valid(good));
    EXPECT_FALSE(composed.is_valid(bad));
}

TEST(QtModelCompositionWithQtRules, InitializerListOfQtPropertyRules)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto model = vd::basic_model<CityObject>{}.with({
        vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }),
        vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) { return zip_checker(s); }),
    });

    CityObject valid("Munich", "80331");
    CityObject bad_zip("Munich", "803");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(bad_zip));
}

// ---------------------------------------------------------------------------
// Group 7: QtFieldFactoryWithQObjectGetters
// vd::field() calls the getter directly (no QVariant involved),
// so string-rule checkers can be passed without lambda wrappers.
// ---------------------------------------------------------------------------

TEST(QtFieldFactoryWithQObjectGetters, FieldWithNonEmptyStringRuleOnQObject)
{
    auto model = vd::basic_model<UserAccount>{}
        .with(vd::field(&UserAccount::username, vd::qt::string_rules::non_empty()));

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount valid  ("alice", "", 30, &addr);
    UserAccount invalid("",      "", 30, &addr);
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldWithEmailLikeCheckerViaLambda)
{
    auto email_checker = vd::qt::string_rules::email_like();
    auto model = vd::basic_model<UserAccount>{}
        .with(vd::field(&UserAccount::email,
            [email_checker](const QString& s) { return email_checker(s); }));

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount valid  ("u", "user@example.com", 30, &addr);
    UserAccount invalid("u", "notanemail",       30, &addr);
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldWithAgeLambda)
{
    auto model = vd::basic_model<UserAccount>{}
        .with(vd::field(&UserAccount::age, [](int a) { return a >= 18 && a <= 120; }));

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount adult("u", "u@u.com", 21,  &addr);
    UserAccount teen ("u", "u@u.com", 16,  &addr);
    UserAccount elder("u", "u@u.com", 121, &addr);
    EXPECT_TRUE(model.is_valid(adult));
    EXPECT_FALSE(model.is_valid(teen));
    EXPECT_FALSE(model.is_valid(elder));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldAndQtPropertyCoexistInSameModel)
{
    // vd::field uses getter pointer; vd::qt::qt_property uses the meta-object
    // system. Both produce rule<UserAccount> and coexist in one basic_model.
    auto email_checker = vd::qt::string_rules::email_like();
    auto model = vd::basic_model<UserAccount>{}
        .with(vd::field(&UserAccount::username, vd::qt::string_rules::non_empty()))
        .with(vd::qt::qt_property<UserAccount>("email",
            [email_checker](const QString& s) { return email_checker(s); }));

    CityObject    city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount both_valid("alice", "alice@mail.com", 25, &addr);
    EXPECT_TRUE(model.is_valid(both_valid));

    UserAccount bad_name("", "alice@mail.com", 25, &addr);
    EXPECT_FALSE(model.is_valid(bad_name));

    UserAccount bad_email("alice", "notanemail", 25, &addr);
    EXPECT_FALSE(model.is_valid(bad_email));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldWithRegexCheckerOnCityZip)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");
    auto model = vd::basic_model<CityObject>{}
        .with(vd::field(&CityObject::zip,
            [zip_checker](const QString& s) { return zip_checker(s); }));

    CityObject valid  ("Berlin", "10115");
    CityObject invalid("Berlin", "1011X");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldCoverage_AllCityFieldsViaGetterPointers)
{
    // Validate both CityObject fields purely via vd::field — no QVariant path.
    auto model = vd::basic_model<CityObject>{}
        .with(vd::field(&CityObject::name, vd::qt::string_rules::non_empty()))
        .with(vd::field(&CityObject::zip,  vd::qt::string_rules::non_empty()));

    CityObject valid  ("Berlin", "10115");
    CityObject no_name("",       "10115");
    CityObject no_zip ("Berlin", "");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(no_name));
    EXPECT_FALSE(model.is_valid(no_zip));
}

// ---------------------------------------------------------------------------
// Group 8: QtComplexApiRequestValidation
// End-to-end validation of an API request object using all Qt rule types:
// uri_like, runtime regex, int bounds, and simple predicates.
// ---------------------------------------------------------------------------

static vd::basic_model<ApiRequestObject> make_api_request_model()
{
    auto endpoint_checker = vd::qt::string_rules::uri_like();
    auto method_checker   = vd::qt::string_rules::regex("GET|POST|PUT|DELETE|PATCH");
    return vd::basic_model<ApiRequestObject>{}
        .with(vd::qt::qt_property<ApiRequestObject>("endpoint",
            [endpoint_checker](const QString& s) { return endpoint_checker(s); }))
        .with(vd::qt::qt_property<ApiRequestObject>("method",
            [method_checker](const QString& s) { return method_checker(s); }))
        .with(vd::qt::qt_property<ApiRequestObject>("timeout_ms",
            [](int t) { return t > 0 && t <= 30000; }))
        .with(vd::qt::qt_property<ApiRequestObject>("auth_token",
            [](const QString& s) { return !s.isEmpty(); }))
        .with(vd::qt::qt_property<ApiRequestObject>("rate_limit",
            [](double r) { return r > 0.0 && r <= 1000.0; }));
}

TEST(QtComplexApiRequestValidation, FullyValidRequestPasses)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/v1/users", "GET", 5000, "Bearer tok123", 100.0);
    EXPECT_TRUE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, InvalidEndpointFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("api.example.com", "GET", 5000, "tok", 100.0); // no scheme
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, InvalidHttpMethodFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "QUERY", 5000, "tok", 100.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, ZeroTimeoutFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "POST", 0, "tok", 100.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, ExcessiveTimeoutFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "POST", 30001, "tok", 100.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, EmptyAuthTokenFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "DELETE", 5000, "", 100.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, RateLimitAtZeroFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "PUT", 5000, "tok", 0.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, RateLimitExceededFails)
{
    auto model = make_api_request_model();
    ApiRequestObject req("https://api.example.com/ep", "PUT", 5000, "tok", 1001.0);
    EXPECT_FALSE(model.is_valid(req));
}

TEST(QtComplexApiRequestValidation, ModelIsReusableAcrossMultipleRequests)
{
    auto model = make_api_request_model();

    ApiRequestObject r1("https://api.example.com/a",  "GET",    1000, "tok1",  50.0);
    ApiRequestObject r2("https://api.example.com/b",  "POST",   2000, "tok2", 200.0);
    ApiRequestObject r3("https://api.example.com/c",  "DELETE",  100, "tok3", 1000.0);
    ApiRequestObject r4("not-a-url",                  "GET",    1000, "tok4",  50.0);
    ApiRequestObject r5("https://api.example.com/d",  "BREW",   1000, "tok5",  50.0);

    EXPECT_TRUE(model.is_valid(r1));
    EXPECT_TRUE(model.is_valid(r2));
    EXPECT_TRUE(model.is_valid(r3));
    EXPECT_FALSE(model.is_valid(r4)); // bad endpoint
    EXPECT_FALSE(model.is_valid(r5)); // BREW is not a valid HTTP method
}

// ---------------------------------------------------------------------------
// Group 9: QtModelCopySafety
// Regression: rule<T>'s forwarding ctor lacked a !same_as<Fn, rule<T>> guard.
// Copying a model triggered the forwarding ctor instead of the copy ctor,
// recursing through std::function construction into a stack overflow.
// ---------------------------------------------------------------------------

TEST(QtModelCopySafety, CopyModelWithPlainPredicateAndCall)
{
    auto m = vd::basic_model<CityObject>{}
        .with(vd::predicate([](const CityObject& c) { return !c.name().isEmpty(); }));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

TEST(QtModelCopySafety, CopyModelWithFieldFactoryAndCall)
{
    auto m = vd::basic_model<CityObject>{}
        .with(vd::field(&CityObject::name, vd::qt::string_rules::non_empty()));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

TEST(QtModelCopySafety, CopyQtPropertyModelAndCall)
{
    auto m = vd::basic_model<CityObject>{}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) { return !s.isEmpty(); }));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

// moc needed because QObject subclasses with Q_OBJECT are defined here.
#include "test_qt_integration.moc"
