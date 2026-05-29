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
    Q_PROPERTY(QString zip READ zip)
public:
    explicit CityObject(const QString& name, const QString& zip, QObject* parent = nullptr) : QObject(parent), m_name(name), m_zip(zip)
    {
    }
    QString name() const
    {
        return m_name;
    }
    QString zip() const
    {
        return m_zip;
    }

private:
    QString m_name, m_zip;
};

class AddressObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString street READ street)
public:
    explicit AddressObject(const QString& street, CityObject* city, QObject* parent = nullptr)
        : QObject(parent), m_street(street), m_city(city)
    {
    }
    QString street() const
    {
        return m_street;
    }
    CityObject* city() const
    {
        return m_city;
    }

private:
    QString m_street;
    CityObject* m_city = nullptr;
};

class UserAccount : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString username READ username)
    Q_PROPERTY(QString email READ email)
    Q_PROPERTY(int age READ age)
public:
    explicit UserAccount(const QString& username, const QString& email, int age, AddressObject* address, QObject* parent = nullptr)
        : QObject(parent), m_username(username), m_email(email), m_age(age), m_address(address)
    {
    }
    QString username() const
    {
        return m_username;
    }
    QString email() const
    {
        return m_email;
    }
    int age() const
    {
        return m_age;
    }
    AddressObject* address() const
    {
        return m_address;
    }

private:
    QString m_username, m_email;
    int m_age = 0;
    AddressObject* m_address = nullptr;
};

// Mutable QObject for bound-model mutation tests.
class MutableSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString token READ token WRITE setToken)
    Q_PROPERTY(int ttl READ ttl WRITE setTtl)
    Q_PROPERTY(QString userId READ userId WRITE setUserId)
public:
    explicit MutableSession(QObject* parent = nullptr) : QObject(parent)
    {
    }
    QString token() const
    {
        return m_token;
    }
    int ttl() const
    {
        return m_ttl;
    }
    QString userId() const
    {
        return m_userId;
    }
    void setToken(const QString& t)
    {
        m_token = t;
    }
    void setTtl(int t)
    {
        m_ttl = t;
    }
    void setUserId(const QString& u)
    {
        m_userId = u;
    }

private:
    QString m_token, m_userId;
    int m_ttl = 0;
};

// QObject representing an API request — all fields immutable after construction.
class ApiRequestObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString endpoint READ endpoint)
    Q_PROPERTY(QString method READ method)
    Q_PROPERTY(int timeout_ms READ timeoutMs)
    Q_PROPERTY(QString auth_token READ authToken)
    Q_PROPERTY(double rate_limit READ rateLimit)
public:
    explicit ApiRequestObject(const QString& endpoint,
        const QString& method,
        int timeout_ms,
        const QString& auth_token,
        double rate_limit,
        QObject* parent = nullptr)
        : QObject(parent), m_endpoint(endpoint), m_method(method), m_timeout_ms(timeout_ms), m_auth_token(auth_token),
          m_rate_limit(rate_limit)
    {
    }
    QString endpoint() const
    {
        return m_endpoint;
    }
    QString method() const
    {
        return m_method;
    }
    int timeoutMs() const
    {
        return m_timeout_ms;
    }
    QString authToken() const
    {
        return m_auth_token;
    }
    double rateLimit() const
    {
        return m_rate_limit;
    }

private:
    QString m_endpoint, m_method, m_auth_token;
    int m_timeout_ms = 0;
    double m_rate_limit = 0.0;
};

// ---------------------------------------------------------------------------
// Global QCoreApplication environment — one per process.
// ---------------------------------------------------------------------------

class QtIntegrationAppEnv : public ::testing::Environment {
public:
    void SetUp() override
    {
        static char prog[] = "test_qt_integration";
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

::testing::Environment* const kQtEnv = ::testing::AddGlobalTestEnvironment(new QtIntegrationAppEnv);

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
    return vd::basic_model<CityObject> {}
        .with(vd::qt::qt_property<CityObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<CityObject>("zip", [zip_checker](const QString& s) {
            return zip_checker(s);
        }));
}

// Takes city_model by value so the caller controls copy vs. move at the call site.
static vd::basic_model<AddressObject> make_address_model(vd::basic_model<CityObject> city_model)
{
    return vd::basic_model<AddressObject> {}
        .with(vd::qt::qt_property<AddressObject>("street",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::predicate([cm = std::move(city_model)](const AddressObject& a) {
            auto* c = a.city();
            return c != nullptr && cm.is_valid(*c);
        }));
}

static vd::basic_model<UserAccount> make_user_model(vd::basic_model<AddressObject> addr_model)
{
    auto email_checker = vd::qt::string_rules::email_like();
    return vd::basic_model<UserAccount> {}
        .with(vd::qt::qt_property<UserAccount>("username",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<UserAccount>("email",
            [email_checker](const QString& s) {
                return email_checker(s);
            }))
        .with(vd::qt::qt_property<UserAccount>("age",
            [](int a) {
                return a >= 18 && a <= 120;
            }))
        .with(vd::predicate([am = std::move(addr_model)](const UserAccount& u) {
            auto* a = u.address();
            return a != nullptr && am.is_valid(*a);
        }));
}

static vd::basic_model<MutableSession> make_session_model()
{
    return vd::basic_model<MutableSession> {}
        .with(vd::qt::qt_property<MutableSession>("token",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<MutableSession>("ttl",
            [](int t) {
                return t > 0;
            }))
        .with(vd::qt::qt_property<MutableSession>("userId", [](const QString& s) {
            return !s.isEmpty();
        }));
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
    CityObject alpha("Berlin", "ABC12");  // not all digits
    CityObject short4("Berlin", "1011");  // 4 digits
    CityObject long6("Berlin", "101150"); // 6 digits
    EXPECT_FALSE(model.is_valid(alpha));
    EXPECT_FALSE(model.is_valid(short4));
    EXPECT_FALSE(model.is_valid(long6));
}

TEST(QtNestedObjectValidation, ValidAddressWithValidCityPasses)
{
    CityObject city("Berlin", "10115");
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
    CityObject bad_city("", "10115");
    AddressObject addr("Unter den Linden 1", &bad_city);
    EXPECT_FALSE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, EmptyStreetFailsAddressModel)
{
    CityObject city("Berlin", "10115");
    AddressObject addr("", &city);
    EXPECT_FALSE(make_address_model(make_city_model()).is_valid(addr));
}

TEST(QtNestedObjectValidation, ValidUserAccountWithFullHierarchyPasses)
{
    CityObject city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount user("alice", "alice@example.com", 30, &addr);
    EXPECT_TRUE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithNullAddressFails)
{
    UserAccount user("alice", "alice@example.com", 30, nullptr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithInvalidEmailFails)
{
    CityObject city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount user("alice", "not-an-email", 30, &addr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountUnderageFailsAgeRule)
{
    CityObject city("Berlin", "10115");
    AddressObject addr("Unter den Linden 1", &city);
    UserAccount user("alice", "alice@example.com", 16, &addr);
    EXPECT_FALSE(make_user_model(make_address_model(make_city_model())).is_valid(user));
}

TEST(QtNestedObjectValidation, UserAccountWithBadZipDeepInChainFails)
{
    CityObject bad_city("Hamburg", "ABCDE"); // zip fails at the deepest level
    AddressObject addr("Jungfernstieg 1", &bad_city);
    UserAccount user("bob", "bob@mail.de", 25, &addr);
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

    CityObject good_city("Paris", "75001");
    AddressObject good_addr("Rue de Rivoli", &good_city);
    EXPECT_TRUE(addr_rule(good_addr));

    CityObject bad_city("", "75001"); // empty name fails
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

    CityObject city("Berlin", "10115");
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
    city_model_strict.add_rule(vd::rule<CityObject>([](const CityObject&) {
        return false;
    }));

    auto city_model_normal = make_city_model();

    auto rule_strict = vd::predicate([city_model_strict](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model_strict.is_valid(*c);
    });
    auto rule_normal = vd::predicate([city_model_normal](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model_normal.is_valid(*c);
    });

    CityObject city("Berlin", "10115");
    AddressObject addr("Street", &city);

    EXPECT_FALSE(rule_strict(addr)); // strict model always fails
    EXPECT_TRUE(rule_normal(addr));  // normal model accepts valid city
}

TEST(QtModelClosureCopyCapture, CapturedModelSurvivesOuterModelCopyAndMove)
{
    auto city_model = make_city_model();

    auto addr_model = vd::basic_model<AddressObject> {}.with(vd::predicate([city_model](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model.is_valid(*c);
    }));

    auto addr_copy = addr_model;            // copy — should carry captured city_model
    auto addr_moved = std::move(addr_copy); // move — addr_copy is now empty (vacuous)

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    EXPECT_TRUE(addr_model.is_valid(addr)); // original still works
    EXPECT_TRUE(addr_moved.is_valid(addr)); // moved model carries the closure intact
}

TEST(QtModelClosureCopyCapture, NestedStringRuleCheckersCapturedInsideClosure)
{
    // Two runtime-constructed string checkers captured inside a model's rule
    // which is itself captured inside another model's rule.
    auto name_checker = vd::qt::string_rules::non_empty();
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto city_model = vd::basic_model<CityObject> {}
                          .with(vd::qt::qt_property<CityObject>("name",
                              [name_checker](const QString& s) {
                                  return name_checker(s);
                              }))
                          .with(vd::qt::qt_property<CityObject>("zip", [zip_checker](const QString& s) {
                              return zip_checker(s);
                          }));

    // city_model's rules each hold a copy of their respective checker.
    // Now capture city_model itself inside an addr predicate.
    auto addr_rule = vd::predicate([city_model](const AddressObject& a) {
        auto* c = a.city();
        return c != nullptr && city_model.is_valid(*c);
    });

    CityObject valid_city("Vienna", "01010");
    AddressObject valid_addr("Ringstrasse 1", &valid_city);
    EXPECT_TRUE(addr_rule(valid_addr));

    CityObject bad_city("Vienna", "X1010");
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

    CityObject good("Berlin", "10115");
    AddressObject good_addr("Street", &good);
    EXPECT_TRUE(rule(good_addr));

    CityObject bad("", "10115"); // empty name fails
    AddressObject bad_addr("Street", &bad);
    EXPECT_FALSE(rule(bad_addr));
}

TEST(QtModelMoveSemantics, RvalueWithChainBuildsCorrectModel)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    // Each with() on an rvalue returns a new model (std::move(*this) internally).
    auto model = vd::basic_model<CityObject> {}
                     .with(vd::qt::qt_property<CityObject>("name",
                         [](const QString& s) {
                             return !s.isEmpty();
                         }))
                     .with(vd::qt::qt_property<CityObject>("zip", [zip_checker](const QString& s) {
                         return zip_checker(s);
                     }));

    CityObject valid("Munich", "80331");
    CityObject bad_zip("Munich", "8033");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(bad_zip));
}

TEST(QtModelMoveSemantics, ModelMovedIntoAnotherModelViaRvalueWith)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto base = vd::basic_model<CityObject> {}.with(vd::qt::qt_property<CityObject>("name", [](const QString& s) {
        return !s.isEmpty();
    }));

    // Rvalue with() on std::move(base) — base is now moved-from.
    auto extended = std::move(base).with(vd::qt::qt_property<CityObject>("zip", [zip_checker](const QString& s) {
        return zip_checker(s);
    }));

    CityObject city("Munich", "80331");
    CityObject bad_zip("Munich", "803");

    EXPECT_TRUE(base.is_valid(city));     // vacuous: base is empty
    EXPECT_TRUE(base.is_valid(bad_zip));  // vacuous: base is empty

    EXPECT_TRUE(extended.is_valid(city)); // both rules present
    EXPECT_FALSE(extended.is_valid(bad_zip));
}

TEST(QtModelMoveSemantics, ThreeLevelChainBuiltWithMovesValidatesCorrectly)
{
    auto city_model = make_city_model();
    auto addr_model = make_address_model(std::move(city_model));
    auto user_model = make_user_model(std::move(addr_model));

    // city_model and addr_model are now empty (moved into closures).
    // user_model holds all three levels inside nested std::function objects.

    CityObject good_city("Hamburg", "20095");
    AddressObject good_addr("Jungfernstieg 1", &good_city);
    UserAccount good_user("bob", "bob@mail.de", 25, &good_addr);
    EXPECT_TRUE(user_model.is_valid(good_user));

    CityObject bad_city("Hamburg", "ABCDE"); // zip fails at the deepest level
    AddressObject bad_addr("Jungfernstieg 1", &bad_city);
    UserAccount bad_user("bob", "bob@mail.de", 25, &bad_addr);
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

    session.setTtl(-1); // session expired
    EXPECT_FALSE(bound.is_valid());

    session.setTtl(900); // refreshed
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
    auto model_token = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("token", [](const QString& s) {
        return !s.isEmpty();
    }));

    auto model_ttl = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("ttl", [](int t) {
        return t > 0;
    }));

    MutableSession session;
    session.setToken("tok");
    session.setTtl(100);
    session.setUserId("u");

    auto bound_token = model_token.bind(session);
    auto bound_ttl = model_ttl.bind(session);

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
    auto inner_model = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("ttl", [](int t) {
        return t > 0;
    }));

    auto outer_model = vd::basic_model<MutableSession> {}
                           .with(vd::qt::qt_property<MutableSession>("token",
                               [](const QString& s) {
                                   return !s.isEmpty();
                               }))
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

    EXPECT_THROW(model.die_if_failed(session), vd::validation_exception);
}

TEST(QtDeadCheck, DoesNotThrowWhenAllPropertiesValid)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok_valid");
    session.setTtl(3600);
    session.setUserId("user_1");

    EXPECT_NO_THROW(model.die_if_failed(session));
}

TEST(QtDeadCheck, BoundModelDeadCheckThrowsAfterMutation)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok");
    session.setTtl(3600);
    session.setUserId("u");

    auto bound = model.bind(session);
    EXPECT_NO_THROW(bound.die_if_failed());

    session.setTtl(0);
    EXPECT_THROW(bound.die_if_failed(), vd::validation_exception);
}

TEST(QtDeadCheck, ThrowsWhenNestedModelDeepInChainFails)
{
    CityObject bad_city("Berlin", "ABCDE"); // bad zip deep in the chain
    AddressObject addr("Street 1", &bad_city);
    UserAccount user("alice", "alice@example.com", 30, &addr);

    EXPECT_THROW(make_user_model(make_address_model(make_city_model())).die_if_failed(user), vd::validation_exception);
}

TEST(QtDeadCheck, ExceptionIsvdValidationException)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("");
    session.setTtl(100);
    session.setUserId("u");

    try {
        model.die_if_failed(session);
        FAIL() << "Expected vd::validation_exception";
    }
    catch(const vd::validation_exception& ex) {
        EXPECT_NE(std::string(ex.what()), "");
    }
    catch(...) {
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
    auto base = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("token", [](const QString& s) {
        return !s.isEmpty();
    }));

    auto extended = vd::basic_model<MutableSession> {}.with(base).with(vd::qt::qt_property<MutableSession>("ttl", [](int t) {
        return t > 0;
    }));

    MutableSession s;
    s.setToken("tok");
    s.setTtl(100);
    s.setUserId("u");
    EXPECT_TRUE(extended.is_valid(s));

    s.setToken(""); // violates base rule
    EXPECT_FALSE(extended.is_valid(s));

    s.setToken("tok");
    s.setTtl(0); // violates extended rule
    EXPECT_FALSE(extended.is_valid(s));
}

TEST(QtModelCompositionWithQtRules, TwoSeparateModelsComposed)
{
    auto model_a = vd::basic_model<MutableSession> {}
                       .with(vd::qt::qt_property<MutableSession>("token",
                           [](const QString& s) {
                               return !s.isEmpty();
                           }))
                       .with(vd::qt::qt_property<MutableSession>("userId", [](const QString& s) {
                           return !s.isEmpty();
                       }));

    auto model_b = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("ttl", [](int t) {
        return t > 0;
    }));

    auto composed = vd::basic_model<MutableSession> {}.with(model_a).with(model_b);

    MutableSession s;
    s.setToken("tok");
    s.setUserId("u");
    s.setTtl(100);
    EXPECT_TRUE(composed.is_valid(s));

    s.setToken("");
    EXPECT_FALSE(composed.is_valid(s));
    s.setToken("tok");
    s.setUserId("");
    EXPECT_FALSE(composed.is_valid(s));
    s.setUserId("u");
    s.setTtl(-1);
    EXPECT_FALSE(composed.is_valid(s));
}

TEST(QtModelCompositionWithQtRules, RvalueWithMergesModelsCorrectly)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto part_a = vd::basic_model<CityObject> {}.with(vd::qt::qt_property<CityObject>("name", [](const QString& s) {
        return !s.isEmpty();
    }));

    auto part_b = vd::basic_model<CityObject> {}.with(vd::qt::qt_property<CityObject>("zip", [zip_checker](const QString& s) {
        return zip_checker(s);
    }));

    auto full = std::move(part_a).with(part_b);

    CityObject valid("Hamburg", "20095");
    CityObject bad_zip("Hamburg", "2009X");
    EXPECT_TRUE(full.is_valid(valid));
    EXPECT_FALSE(full.is_valid(bad_zip));
}

TEST(QtModelCompositionWithQtRules, EmptyModelComposedWithRuledModel)
{
    auto empty = vd::basic_model<CityObject> {};
    auto ruled = vd::basic_model<CityObject> {}.with(vd::qt::qt_property<CityObject>("name", [](const QString& s) {
        return !s.isEmpty();
    }));

    auto composed = vd::basic_model<CityObject> {}.with(empty).with(ruled);

    CityObject good("Berlin", "10115");
    CityObject bad("", "10115");
    EXPECT_TRUE(composed.is_valid(good));
    EXPECT_FALSE(composed.is_valid(bad));
}

TEST(QtModelCompositionWithQtRules, InitializerListOfQtPropertyRules)
{
    auto zip_checker = vd::qt::string_rules::regex(R"(\d{5})");

    auto model = vd::basic_model<CityObject> {}.with({
        vd::qt::qt_property<CityObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }),
        vd::qt::qt_property<CityObject>("zip",
            [zip_checker](const QString& s) {
                return zip_checker(s);
            }),
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
    auto model = vd::basic_model<UserAccount> {}.with(vd::field(&UserAccount::username, vd::qt::string_rules::non_empty()));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount valid("alice", "", 30, &addr);
    UserAccount invalid("", "", 30, &addr);
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldWithEmailLikeCheckerViaLambda)
{
    auto email_checker = vd::qt::string_rules::email_like();
    auto model = vd::basic_model<UserAccount> {}.with(vd::field(&UserAccount::email, [email_checker](const QString& s) {
        return email_checker(s);
    }));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount valid("u", "user@example.com", 30, &addr);
    UserAccount invalid("u", "notanemail", 30, &addr);
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldWithAgeLambda)
{
    auto model = vd::basic_model<UserAccount> {}.with(vd::field(&UserAccount::age, [](int a) {
        return a >= 18 && a <= 120;
    }));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);

    UserAccount adult("u", "u@u.com", 21, &addr);
    UserAccount teen("u", "u@u.com", 16, &addr);
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
    auto model = vd::basic_model<UserAccount> {}
                     .with(vd::field(&UserAccount::username, vd::qt::string_rules::non_empty()))
                     .with(vd::qt::qt_property<UserAccount>("email", [email_checker](const QString& s) {
                         return email_checker(s);
                     }));

    CityObject city("Berlin", "10115");
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
    auto model = vd::basic_model<CityObject> {}.with(vd::field(&CityObject::zip, [zip_checker](const QString& s) {
        return zip_checker(s);
    }));

    CityObject valid("Berlin", "10115");
    CityObject invalid("Berlin", "1011X");
    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(invalid));
}

TEST(QtFieldFactoryWithQObjectGetters, FieldCoverage_AllCityFieldsViaGetterPointers)
{
    // Validate both CityObject fields purely via vd::field — no QVariant path.
    auto model = vd::basic_model<CityObject> {}
                     .with(vd::field(&CityObject::name, vd::qt::string_rules::non_empty()))
                     .with(vd::field(&CityObject::zip, vd::qt::string_rules::non_empty()));

    CityObject valid("Berlin", "10115");
    CityObject no_name("", "10115");
    CityObject no_zip("Berlin", "");
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
    auto method_checker = vd::qt::string_rules::regex("GET|POST|PUT|DELETE|PATCH");
    return vd::basic_model<ApiRequestObject> {}
        .with(vd::qt::qt_property<ApiRequestObject>("endpoint",
            [endpoint_checker](const QString& s) {
                return endpoint_checker(s);
            }))
        .with(vd::qt::qt_property<ApiRequestObject>("method",
            [method_checker](const QString& s) {
                return method_checker(s);
            }))
        .with(vd::qt::qt_property<ApiRequestObject>("timeout_ms",
            [](int t) {
                return t > 0 && t <= 30000;
            }))
        .with(vd::qt::qt_property<ApiRequestObject>("auth_token",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<ApiRequestObject>("rate_limit", [](double r) {
            return r > 0.0 && r <= 1000.0;
        }));
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

    ApiRequestObject r1("https://api.example.com/a", "GET", 1000, "tok1", 50.0);
    ApiRequestObject r2("https://api.example.com/b", "POST", 2000, "tok2", 200.0);
    ApiRequestObject r3("https://api.example.com/c", "DELETE", 100, "tok3", 1000.0);
    ApiRequestObject r4("not-a-url", "GET", 1000, "tok4", 50.0);
    ApiRequestObject r5("https://api.example.com/d", "BREW", 1000, "tok5", 50.0);

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
    auto m = vd::basic_model<CityObject> {}.with(vd::predicate([](const CityObject& c) {
        return !c.name().isEmpty();
    }));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

TEST(QtModelCopySafety, CopyModelWithFieldFactoryAndCall)
{
    auto m = vd::basic_model<CityObject> {}.with(vd::field(&CityObject::name, vd::qt::string_rules::non_empty()));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

TEST(QtModelCopySafety, CopyQtPropertyModelAndCall)
{
    auto m = vd::basic_model<CityObject> {}.with(vd::qt::qt_property<CityObject>("name", [](const QString& s) {
        return !s.isEmpty();
    }));
    auto m2 = m;
    CityObject city("Berlin", "10115");
    EXPECT_TRUE(m2.is_valid(city));
}

// ---------------------------------------------------------------------------
// QObject hierarchy for Groups 10 and 11.
//
// DepartmentObject  — leaf node, only value-type Q_PROPERTYs.
// EmployeeObject    — exposes DepartmentObject* as a Q_PROPERTY.
// ProjectObject     — exposes EmployeeObject* and DepartmentObject* as Q_PROPERTYs.
// MutableEmployee   — writable counterpart of EmployeeObject for mutation tests.
//
// These classes model a realistic domain with QObject pointer composition so
// that qt_property() is tested against fields whose Q_PROPERTY type is itself
// a QObject subclass pointer.
// ---------------------------------------------------------------------------

class DepartmentObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int headcount READ headcount)
public:
    explicit DepartmentObject(const QString& name, int headcount, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_headcount(headcount)
    {
    }
    QString name() const
    {
        return m_name;
    }
    int headcount() const
    {
        return m_headcount;
    }

private:
    QString m_name;
    int m_headcount = 0;
};

class EmployeeObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString role READ role)
    Q_PROPERTY(DepartmentObject* department READ department)
public:
    explicit EmployeeObject(const QString& name, const QString& role, DepartmentObject* dept, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_role(role), m_department(dept)
    {
    }
    QString name() const
    {
        return m_name;
    }
    QString role() const
    {
        return m_role;
    }
    DepartmentObject* department() const
    {
        return m_department;
    }

private:
    QString m_name, m_role;
    DepartmentObject* m_department = nullptr;
};

class ProjectObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(EmployeeObject* lead READ lead)
    Q_PROPERTY(DepartmentObject* owner_dept READ ownerDept)
public:
    explicit ProjectObject(const QString& title, EmployeeObject* lead, DepartmentObject* owner, QObject* parent = nullptr)
        : QObject(parent), m_title(title), m_lead(lead), m_ownerDept(owner)
    {
    }
    QString title() const
    {
        return m_title;
    }
    EmployeeObject* lead() const
    {
        return m_lead;
    }
    DepartmentObject* ownerDept() const
    {
        return m_ownerDept;
    }

private:
    QString m_title;
    EmployeeObject* m_lead = nullptr;
    DepartmentObject* m_ownerDept = nullptr;
};

class MutableEmployee : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QString role READ role WRITE setRole)
    Q_PROPERTY(DepartmentObject* department READ department WRITE setDepartment)
public:
    explicit MutableEmployee(QObject* parent = nullptr) : QObject(parent)
    {
    }
    QString name() const
    {
        return m_name;
    }
    QString role() const
    {
        return m_role;
    }
    DepartmentObject* department() const
    {
        return m_department;
    }
    void setName(const QString& n)
    {
        m_name = n;
    }
    void setRole(const QString& r)
    {
        m_role = r;
    }
    void setDepartment(DepartmentObject* d)
    {
        m_department = d;
    }

private:
    QString m_name, m_role;
    DepartmentObject* m_department = nullptr;
};

// ---------------------------------------------------------------------------
// Model factories for Groups 10 and 11
// ---------------------------------------------------------------------------

static vd::basic_model<DepartmentObject> make_department_model()
{
    return vd::basic_model<DepartmentObject> {}
        .with(vd::qt::qt_property<DepartmentObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<DepartmentObject>("headcount", [](int h) {
            return h > 0;
        }));
}

// Approach A: qt_property with DepartmentObject* as the checker argument type.
// PropT is deduced as DepartmentObject*.  The QVariant returned by property()
// stores DepartmentObject* exactly, so canConvert<DepartmentObject*>() is an
// exact-type hit — no upcast or registered conversion needed.
static vd::basic_model<EmployeeObject> make_employee_model_via_typed_ptr(vd::basic_model<DepartmentObject> dept_model)
{
    return vd::basic_model<EmployeeObject> {}
        .with(vd::qt::qt_property<EmployeeObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<EmployeeObject>("role",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<EmployeeObject>("department", [dm = std::move(dept_model)](DepartmentObject* d) {
            return d != nullptr && dm.is_valid(*d);
        }));
}

// Approach B: qt_property with QObject* as the checker argument type.
// PropT = QObject*; canConvert<QObject*>() for a QVariant holding DepartmentObject*
// is Qt-version-dependent — on some builds this path silently returns false
// because QVariant does not register automatic pointer upcasts.
// qobject_cast is used inside the checker to safely attempt the downcast.
static vd::basic_model<EmployeeObject> make_employee_model_via_qobject_ptr(vd::basic_model<DepartmentObject> dept_model)
{
    return vd::basic_model<EmployeeObject> {}
        .with(vd::qt::qt_property<EmployeeObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<EmployeeObject>("role",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<EmployeeObject>("department", [dm = std::move(dept_model)](QObject* d) {
            auto* dept = qobject_cast<DepartmentObject*>(d);
            return dept != nullptr && dm.is_valid(*dept);
        }));
}

// Approach C: vd::predicate bypassing QVariant entirely — calls the C++ getter
// directly.  This is the established safe pattern from Groups 1-5.
static vd::basic_model<EmployeeObject> make_employee_model_via_predicate(vd::basic_model<DepartmentObject> dept_model)
{
    return vd::basic_model<EmployeeObject> {}
        .with(vd::qt::qt_property<EmployeeObject>("name",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<EmployeeObject>("role",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::predicate([dm = std::move(dept_model)](const EmployeeObject& e) {
            auto* dept = e.department();
            return dept != nullptr && dm.is_valid(*dept);
        }));
}

// Project model: two QObject* Q_PROPERTYs validated via typed-pointer checkers.
static vd::basic_model<ProjectObject> make_project_model(
    vd::basic_model<EmployeeObject> emp_model, vd::basic_model<DepartmentObject> dept_model)
{
    return vd::basic_model<ProjectObject> {}
        .with(vd::qt::qt_property<ProjectObject>("title",
            [](const QString& s) {
                return !s.isEmpty();
            }))
        .with(vd::qt::qt_property<ProjectObject>("lead",
            [em = std::move(emp_model)](EmployeeObject* e) {
                return e != nullptr && em.is_valid(*e);
            }))
        .with(vd::qt::qt_property<ProjectObject>("owner_dept", [dm = std::move(dept_model)](DepartmentObject* d) {
            return d != nullptr && dm.is_valid(*d);
        }));
}

// ---------------------------------------------------------------------------
// Group 10: QtQObjectPointerPropertyValidation
//
// Exercises qt_property() when the Q_PROPERTY type is a QObject subclass
// pointer (DepartmentObject*).  Three access paths are compared:
//
//  A) Typed-pointer checker  — PropT = DepartmentObject*, exact QVariant match.
//  B) QObject* upcast checker — PropT = QObject*, Qt-version-dependent.
//  C) vd::predicate           — bypasses QVariant, calls C++ getter directly.
//
// The suite verifies that path A and C agree, documents path B's behaviour,
// and covers null/invalid pointer edge cases plus mutation tracking.
// ---------------------------------------------------------------------------

TEST(QtQObjectPointerPropertyValidation, TypedPtrCheckerValidEmployeePasses)
{
    auto model = make_employee_model_via_typed_ptr(make_department_model());
    DepartmentObject dept("Engineering", 42);
    EmployeeObject emp("Alice", "Engineer", &dept);
    EXPECT_TRUE(model.is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrCheckerNullDeptFails)
{
    // Checker receives nullptr from the QVariant; the model must reject it.
    auto model = make_employee_model_via_typed_ptr(make_department_model());
    EmployeeObject emp("Alice", "Engineer", nullptr);
    EXPECT_FALSE(model.is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrCheckerInvalidDeptFails)
{
    auto model = make_employee_model_via_typed_ptr(make_department_model());

    DepartmentObject empty_name("", 5); // name rule fails
    EmployeeObject emp1("Bob", "Manager", &empty_name);
    EXPECT_FALSE(model.is_valid(emp1));

    DepartmentObject zero_hc("HR", 0); // headcount rule fails
    EmployeeObject emp2("Bob", "Manager", &zero_hc);
    EXPECT_FALSE(model.is_valid(emp2));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrCheckerEmptyEmployeeNameFails)
{
    auto model = make_employee_model_via_typed_ptr(make_department_model());
    DepartmentObject dept("Engineering", 10);
    EmployeeObject emp("", "Dev", &dept); // employee name rule fails
    EXPECT_FALSE(model.is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrAndPredicateApproachesAgreeOnValidEmployee)
{
    DepartmentObject dept("Engineering", 10);
    EmployeeObject emp("Dave", "Dev", &dept);

    EXPECT_TRUE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_TRUE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrAndPredicateApproachesAgreeOnNullDept)
{
    EmployeeObject emp("Dave", "Dev", nullptr);

    EXPECT_FALSE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, TypedPtrAndPredicateApproachesAgreeOnInvalidDept)
{
    DepartmentObject bad("", 0);
    EmployeeObject emp("Dave", "Dev", &bad);

    EXPECT_FALSE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, HybridModelScalarQtPropertyAndPointerPredicate)
{
    // Scalar Q_PROPERTYs validated via qt_property(), pointer property via
    // vd::predicate.  Both rule types coexist in the same basic_model<>.
    auto dept_model = make_department_model();
    auto model = vd::basic_model<EmployeeObject> {}
                     .with(vd::qt::qt_property<EmployeeObject>("name",
                         [](const QString& s) {
                             return !s.isEmpty();
                         }))
                     .with(vd::qt::qt_property<EmployeeObject>("role",
                         [](const QString& s) {
                             return !s.isEmpty();
                         }))
                     .with(vd::predicate([dm = std::move(dept_model)](const EmployeeObject& e) {
                         auto* d = e.department();
                         return d != nullptr && dm.is_valid(*d);
                     }));

    DepartmentObject valid_dept("Sales", 5);
    EmployeeObject valid("Eve", "Lead", &valid_dept);
    EXPECT_TRUE(model.is_valid(valid));

    EmployeeObject bad_name("", "Lead", &valid_dept);
    EXPECT_FALSE(model.is_valid(bad_name));

    EmployeeObject bad_role("Eve", "", &valid_dept);
    EXPECT_FALSE(model.is_valid(bad_role));

    EmployeeObject null_dept("Eve", "Lead", nullptr);
    EXPECT_FALSE(model.is_valid(null_dept));
}

TEST(QtQObjectPointerPropertyValidation, BoundMutableEmployeeReflectsDepartmentSwap)
{
    // basic_bound_model stores const T* — re-evaluates on each call, so swapping
    // the department pointer is immediately visible.
    auto model = vd::basic_model<MutableEmployee> {}
                     .with(vd::qt::qt_property<MutableEmployee>("name",
                         [](const QString& s) {
                             return !s.isEmpty();
                         }))
                     .with(vd::qt::qt_property<MutableEmployee>("department", [dm = make_department_model()](DepartmentObject* d) {
                         return d != nullptr && dm.is_valid(*d);
                     }));

    DepartmentObject good("Engineering", 10);
    DepartmentObject bad("", 0);

    MutableEmployee emp;
    emp.setName("Frank");
    emp.setDepartment(&good);

    auto bound = model.bind(emp);
    EXPECT_TRUE(bound.is_valid());

    emp.setDepartment(&bad);
    EXPECT_FALSE(bound.is_valid());

    emp.setDepartment(&good);
    EXPECT_TRUE(bound.is_valid());

    emp.setDepartment(nullptr);
    EXPECT_FALSE(bound.is_valid());
}

TEST(QtQObjectPointerPropertyValidation, DeadCheckThrowsWhenPointerPropertyInvalid)
{
    auto model = make_employee_model_via_typed_ptr(make_department_model());

    DepartmentObject bad("", 0);
    EmployeeObject emp("Grace", "Architect", &bad);

    EXPECT_THROW(model.die_if_failed(emp), vd::validation_exception);
}

// Approach B tests: qt_property with QObject* checker (PropT = QObject*).
// In Qt 6, QVariant::canConvert<QObject*>() for a QVariant that stores
// DepartmentObject* succeeds via the built-in QObject pointer metatype
// support, so qobject_cast inside the checker can recover the typed pointer.
// These tests verify Approach B produces the same results as Approaches A/C.

TEST(QtQObjectPointerPropertyValidation, QObjectPtrCheckerValidEmployeePasses)
{
    auto model = make_employee_model_via_qobject_ptr(make_department_model());
    DepartmentObject dept("Engineering", 42);
    EmployeeObject emp("Alice", "Engineer", &dept);
    EXPECT_TRUE(model.is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, QObjectPtrCheckerNullDeptFails)
{
    auto model = make_employee_model_via_qobject_ptr(make_department_model());
    EmployeeObject emp("Alice", "Engineer", nullptr);
    EXPECT_FALSE(model.is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, AllThreeApproachesAgreeOnValidEmployee)
{
    DepartmentObject dept("Engineering", 10);
    EmployeeObject emp("Dave", "Dev", &dept);

    EXPECT_TRUE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_TRUE(make_employee_model_via_qobject_ptr(make_department_model()).is_valid(emp));
    EXPECT_TRUE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, AllThreeApproachesAgreeOnNullDept)
{
    EmployeeObject emp("Dave", "Dev", nullptr);

    EXPECT_FALSE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_qobject_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

TEST(QtQObjectPointerPropertyValidation, AllThreeApproachesAgreeOnInvalidDept)
{
    DepartmentObject bad("", 0);
    EmployeeObject emp("Dave", "Dev", &bad);

    EXPECT_FALSE(make_employee_model_via_typed_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_qobject_ptr(make_department_model()).is_valid(emp));
    EXPECT_FALSE(make_employee_model_via_predicate(make_department_model()).is_valid(emp));
}

// ---------------------------------------------------------------------------
// Group 11: QtQObjectPointerPropertyChaining
//
// ProjectObject has two QObject* Q_PROPERTYs — lead (EmployeeObject*) and
// owner_dept (DepartmentObject*).  EmployeeObject in turn has its own
// DepartmentObject* Q_PROPERTY.  The full validation chain is:
//
//   ProjectObject.title      — qt_property, QString
//   ProjectObject.lead       — qt_property, EmployeeObject* (carries its own model)
//   ProjectObject.owner_dept — qt_property, DepartmentObject* (carries dept model)
//     EmployeeObject.name    — qt_property, QString
//     EmployeeObject.role    — qt_property, QString
//     EmployeeObject.department — qt_property, DepartmentObject* (deepest level)
//       DepartmentObject.name      — qt_property, QString
//       DepartmentObject.headcount — qt_property, int
//
// Every pointer step uses Approach A (typed-pointer checker) so the whole
// chain goes through the QVariant path without predicate bypass.
// ---------------------------------------------------------------------------

TEST(QtQObjectPointerPropertyChaining, FullyValidProjectPasses)
{
    DepartmentObject dept("R&D", 15);
    EmployeeObject lead("Grace", "PM", &dept);
    ProjectObject project("Atlas", &lead, &dept);

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_TRUE(model.is_valid(project));
}

TEST(QtQObjectPointerPropertyChaining, NullLeadFails)
{
    DepartmentObject dept("R&D", 15);
    ProjectObject project("Atlas", nullptr, &dept);

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_FALSE(model.is_valid(project));
}

TEST(QtQObjectPointerPropertyChaining, NullOwnerDeptFails)
{
    DepartmentObject dept("R&D", 15);
    EmployeeObject lead("Grace", "PM", &dept);
    ProjectObject project("Atlas", &lead, nullptr);

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_FALSE(model.is_valid(project));
}

TEST(QtQObjectPointerPropertyChaining, EmptyProjectTitleFails)
{
    DepartmentObject dept("R&D", 15);
    EmployeeObject lead("Grace", "PM", &dept);
    ProjectObject project("", &lead, &dept); // title rule fails

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_FALSE(model.is_valid(project));
}

TEST(QtQObjectPointerPropertyChaining, BadDeptDeepInLeadChainFails)
{
    // DepartmentObject fails at the third level — deepest node.
    DepartmentObject bad_lead_dept("", 0);
    EmployeeObject lead("Grace", "PM", &bad_lead_dept);
    DepartmentObject owner_dept("R&D", 5);
    ProjectObject project("Atlas", &lead, &owner_dept);

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_FALSE(model.is_valid(project));
}

TEST(QtQObjectPointerPropertyChaining, LeadAndOwnerDeptValidatedIndependently)
{
    DepartmentObject good_dept("Engineering", 10);
    DepartmentObject bad_dept("", 0);

    // lead passes, owner_dept fails
    EmployeeObject good_lead("Hank", "Arch", &good_dept);
    ProjectObject p_bad_owner("X", &good_lead, &bad_dept);

    // lead fails (its dept is bad), owner_dept passes
    EmployeeObject bad_lead("Hank", "Arch", &bad_dept);
    ProjectObject p_bad_lead("X", &bad_lead, &good_dept);

    auto make_model = [&]() {
        return make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    };

    EXPECT_FALSE(make_model().is_valid(p_bad_owner));
    EXPECT_FALSE(make_model().is_valid(p_bad_lead));
}

TEST(QtQObjectPointerPropertyChaining, ThreeLevelDeadCheckThrowsOnDeepFailure)
{
    DepartmentObject bad_dept("", 0); // fails deepest level
    EmployeeObject lead("Grace", "PM", &bad_dept);
    DepartmentObject owner_dept("R&D", 5);
    ProjectObject project("Atlas", &lead, &owner_dept);

    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());
    EXPECT_THROW(model.die_if_failed(project), vd::validation_exception);
}

TEST(QtQObjectPointerPropertyChaining, CopiedProjectModelIsIndependentOfOriginal)
{
    auto model = make_project_model(make_employee_model_via_typed_ptr(make_department_model()), make_department_model());

    auto copy = model; // deep copy — each level's closure is independently copied

    DepartmentObject dept("R&D", 15);
    EmployeeObject lead("Grace", "PM", &dept);
    ProjectObject valid("Atlas", &lead, &dept);

    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_TRUE(copy.is_valid(valid));

    // Extra rule on the original that rejects everything.
    model.add_rule(vd::rule<ProjectObject>([](const ProjectObject&) {
        return false;
    }));
    EXPECT_FALSE(model.is_valid(valid));
    EXPECT_TRUE(copy.is_valid(valid)); // copy is unaffected
}

TEST(QtQObjectPointerPropertyChaining, ProjectModelBuiltWithRvalueChainValidatesCorrectly)
{
    // Entire model built via rvalue .with() chain — no named intermediates.
    DepartmentObject dept("Engineering", 8);
    EmployeeObject lead("Ida", "TL", &dept);
    ProjectObject valid("Phoenix", &lead, &dept);
    ProjectObject bad_title("", &lead, &dept);

    auto dept_model_a = make_department_model();
    auto dept_model_b = make_department_model();

    auto model = vd::basic_model<ProjectObject> {}
                     .with(vd::qt::qt_property<ProjectObject>("title",
                         [](const QString& s) {
                             return !s.isEmpty();
                         }))
                     .with(vd::qt::qt_property<ProjectObject>("lead",
                         [em = make_employee_model_via_typed_ptr(std::move(dept_model_a))](EmployeeObject* e) {
                             return e != nullptr && em.is_valid(*e);
                         }))
                     .with(vd::qt::qt_property<ProjectObject>("owner_dept", [dm = std::move(dept_model_b)](DepartmentObject* d) {
                         return d != nullptr && dm.is_valid(*d);
                     }));

    EXPECT_TRUE(model.is_valid(valid));
    EXPECT_FALSE(model.is_valid(bad_title));
}

// ---------------------------------------------------------------------------
// Group 12: QtResultApiInspection
//
// Tests that vd::result from Qt model validation carries both the pass/fail
// state and, where available, diagnostic messages.
//
// Message propagation rules for Qt models:
//  - vd::qt::qt_property() with a bool checker synthesises the message
//    "QProperty <name> validation failed" and stores it in failed_rules.
//  - vd::qt::qt_property() with a vd::result-returning checker forwards the
//    checker's own messages into failed_rules.
//  - vd::field() with a Qt getter + a vd::result-returning checker (e.g.
//    vd::qt::string_rules::*) propagates the checker message into failed_rules.
//  - vd::field() with a Qt getter + a bool lambda synthesises the message
//    "Field '<name>' failed validation" using the supplied field name.
// ---------------------------------------------------------------------------

TEST(QtResultApiInspection, ValidQtModel_Result_IsValidAndEmptyFailedRules)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken("tok_valid");
    session.setTtl(3600);
    session.setUserId("user_1");

    auto r = model.is_valid(session);
    EXPECT_TRUE(r.is_valid);
    EXPECT_TRUE(r.failed_rules.empty());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(QtResultApiInspection, FailingQtPropertyWithBoolChecker_ResultIsInvalid_OperatorBoolFalse)
{
    auto model = make_session_model();
    MutableSession session;
    session.setToken(""); // empty — fails bool checker
    session.setTtl(3600);
    session.setUserId("user_1");

    auto r = model.is_valid(session);
    EXPECT_FALSE(r.is_valid);
    EXPECT_FALSE(static_cast<bool>(r));
}

// qt_property() with a bool checker synthesises the message
// "QProperty <name> validation failed" and puts it in failed_rules.
TEST(QtResultApiInspection, FailingQtPropertyWithBoolChecker_FailedRulesContainsPropertyName)
{
    auto model = vd::basic_model<MutableSession> {}.with(vd::qt::qt_property<MutableSession>("token", [](const QString& s) {
        return !s.isEmpty();
    }));

    MutableSession session;
    session.setToken(""); // fails

    auto r = model.is_valid(session);
    EXPECT_FALSE(r);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_NE(r.failed_rules[0].find("token"), std::string::npos);
}

// vd::field with a Qt getter + vd::result-returning checker preserves the
// checker's message in failed_rules.
TEST(QtResultApiInspection, VdField_WithQtStringRule_EmptyString_MessagePreserved)
{
    auto model = vd::basic_model<UserAccount> {}.with(vd::field(&UserAccount::username, vd::qt::string_rules::non_empty()));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);
    UserAccount user("", "u@u.com", 30, &addr); // empty username — fails

    auto r = model.is_valid(user);
    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_NE(r.failed_rules[0].find("string must be non-empty"), std::string::npos);
}

// vd::field with a named field and bool lambda uses the field name in the message.
TEST(QtResultApiInspection, VdField_NamedWithBoolChecker_Fails_MessageContainsFieldName)
{
    auto model = vd::basic_model<UserAccount> {}.with(vd::field("username", &UserAccount::username, [](const QString& s) {
        return !s.isEmpty();
    }));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);
    UserAccount user("", "u@u.com", 30, &addr);

    auto r = model.is_valid(user);
    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_NE(r.failed_rules[0].find("username"), std::string::npos);
}

// format() produces a non-empty report when failed_rules are populated.
TEST(QtResultApiInspection, FailedResult_VdField_Format_ContainsCheckerMessage)
{
    auto model = vd::basic_model<UserAccount> {}.with(vd::field("username", &UserAccount::username, vd::qt::string_rules::non_empty()));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);
    UserAccount user("", "u@u.com", 30, &addr);

    auto r = model.is_valid(user);
    ASSERT_FALSE(r.is_valid);
    std::string report = r.format();
    EXPECT_NE(report.find("string must be non-empty"), std::string::npos);
}

// die_if_failed() on a failed vd::result from a Qt model throws and the
// exception message is non-empty.
TEST(QtResultApiInspection, DieIfFailed_OnQtModelFailedResult_ThrowsWithNonEmptyMessage)
{
    auto model = vd::basic_model<UserAccount> {}.with(vd::field("username", &UserAccount::username, vd::qt::string_rules::non_empty()));

    CityObject city("Berlin", "10115");
    AddressObject addr("Street 1", &city);
    UserAccount user("", "u@u.com", 30, &addr);

    auto r = model.is_valid(user);
    try {
        r.die_if_failed();
        FAIL() << "Expected vd::validation_exception";
    }
    catch(const vd::validation_exception& ex) {
        EXPECT_NE(std::string(ex.what()), "");
    }
}

// moc needed because QObject subclasses with Q_OBJECT are defined here.
#include "test_qt_integration.moc"
