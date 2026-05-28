#include <gtest/gtest.h>
#include <vd.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================================
// Domain structures — realistic application data types
// ============================================================================

struct UserRegistration {
    std::string username;
    std::string email;
    std::int32_t age = 0;
    std::string password;
};

struct ServerConfig {
    std::string host;
    std::int32_t port = 0;
    std::int32_t max_connections = 0;
    double timeout_seconds = 0.0;
};

struct Product {
    std::string sku;
    std::string name;
    double price = 0.0;
    std::int32_t stock = 0;
    double discount_percent = 0.0;
};

struct Transfer {
    double amount = 0.0;
    double fee = 0.0;
    std::string from_account;
    std::string to_account;
};

struct UserProfile {
    std::string username;
    std::string email;
    std::string bio;
    std::string admin_token;
};

// ============================================================================
// Model factories — "build once, validate many" pattern
// ============================================================================

static vd::basic_model<UserRegistration> make_registration_model()
{
    return vd::basic_model<UserRegistration> {}
        .with(vd::member(&UserRegistration::username, vd::string_rules::non_empty()))
        .with(vd::member(&UserRegistration::email, vd::string_rules::email_like()))
        .with(vd::member(&UserRegistration::age, vd::int_bounds::inclusive(18, 120)))
        .with(vd::predicate([](const UserRegistration& u) {
            return u.password.size() >= 8;
        }));
}

static vd::basic_model<ServerConfig> make_server_config_model()
{
    return vd::basic_model<ServerConfig> {}
        .with(vd::member(&ServerConfig::host, vd::string_rules::non_empty()))
        .with(vd::member(&ServerConfig::port, vd::int_bounds::inclusive(1, 65535)))
        .with(vd::member(&ServerConfig::max_connections, vd::int_bounds::inclusive(1, 10000)))
        .with(vd::member(&ServerConfig::timeout_seconds, vd::double_bounds::greater_than(0.0)));
}

// ============================================================================
// 1. UserRegistration — service-boundary validation
// ============================================================================

class UserRegistrationTest : public ::testing::Test {
protected:
    vd::basic_model<UserRegistration> model = make_registration_model();
    UserRegistration valid { "alice_42", "alice@example.com", 25, "s3cr3tPass!" };
};

TEST_F(UserRegistrationTest, ValidInput_Passes)
{
    EXPECT_TRUE(model.is_valid(valid));
}

TEST_F(UserRegistrationTest, EmptyUsername_Fails)
{
    auto u = valid;
    u.username = "";
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, InvalidEmail_NoAt_Fails)
{
    auto u = valid;
    u.email = "notanemail";
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, InvalidEmail_NoDomain_Fails)
{
    auto u = valid;
    u.email = "user@";
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, AgeLowerBoundary_18_Passes)
{
    auto u = valid;
    u.age = 18;
    EXPECT_TRUE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, AgeUpperBoundary_120_Passes)
{
    auto u = valid;
    u.age = 120;
    EXPECT_TRUE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, AgeJustBelow18_Fails)
{
    auto u = valid;
    u.age = 17;
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, AgeJustAbove120_Fails)
{
    auto u = valid;
    u.age = 121;
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, ZeroAge_Fails)
{
    auto u = valid;
    u.age = 0;
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, NegativeAge_Fails)
{
    auto u = valid;
    u.age = -1;
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, Password7Chars_Fails)
{
    auto u = valid;
    u.password = "abc1234";
    EXPECT_FALSE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, Password8Chars_Passes)
{
    auto u = valid;
    u.password = "abc12345";
    EXPECT_TRUE(model.is_valid(u));
}

TEST_F(UserRegistrationTest, AllFieldsInvalid_StillFails)
{
    EXPECT_FALSE(model.is_valid(UserRegistration { "", "bad-email", 15, "short" }));
}

TEST_F(UserRegistrationTest, DieIfFailed_Valid_DoesNotThrow)
{
    EXPECT_NO_THROW(model.die_if_failed(valid));
}

TEST_F(UserRegistrationTest, DieIfFailed_Invalid_ThrowsValidationException)
{
    EXPECT_THROW(model.die_if_failed(UserRegistration { "", "bad", 15, "x" }), vd::validation_exception);
}

TEST_F(UserRegistrationTest, DieIfFailed_ExceptionCaughtAsStdException)
{
    EXPECT_THROW(model.die_if_failed(UserRegistration { "", "bad", 15, "x" }), std::exception);
}

TEST_F(UserRegistrationTest, ServiceBoundaryPattern_InvalidInput_BlocksProcessing)
{
    bool processed = false;
    UserRegistration bad { "user", "invalid@@@email", 25, "goodpassword" };

    try {
        model.die_if_failed(bad);
        processed = true;
    }
    catch(const vd::validation_exception&) {
    }

    EXPECT_FALSE(processed);
}

TEST_F(UserRegistrationTest, ServiceBoundaryPattern_ValidInput_Proceeds)
{
    bool processed = false;

    try {
        model.die_if_failed(valid);
        processed = true;
    }
    catch(const vd::validation_exception&) {
        FAIL() << "Valid registration must not throw";
    }

    EXPECT_TRUE(processed);
}

// ============================================================================
// 2. ServerConfig — infrastructure configuration validation
// ============================================================================

class ServerConfigTest : public ::testing::Test {
protected:
    vd::basic_model<ServerConfig> model = make_server_config_model();
    ServerConfig valid { "127.0.0.1", 8080, 256, 30.0 };
};

TEST_F(ServerConfigTest, ValidConfig_Passes)
{
    EXPECT_TRUE(model.is_valid(valid));
}

TEST_F(ServerConfigTest, Port_1_Passes)
{
    auto c = valid;
    c.port = 1;
    EXPECT_TRUE(model.is_valid(c));
}
TEST_F(ServerConfigTest, Port_65535_Passes)
{
    auto c = valid;
    c.port = 65535;
    EXPECT_TRUE(model.is_valid(c));
}
TEST_F(ServerConfigTest, Port_0_Fails)
{
    auto c = valid;
    c.port = 0;
    EXPECT_FALSE(model.is_valid(c));
}
TEST_F(ServerConfigTest, Port_65536_Fails)
{
    auto c = valid;
    c.port = 65536;
    EXPECT_FALSE(model.is_valid(c));
}
TEST_F(ServerConfigTest, Port_Negative_Fails)
{
    auto c = valid;
    c.port = -1;
    EXPECT_FALSE(model.is_valid(c));
}

TEST_F(ServerConfigTest, MaxConns_1_Passes)
{
    auto c = valid;
    c.max_connections = 1;
    EXPECT_TRUE(model.is_valid(c));
}
TEST_F(ServerConfigTest, MaxConns_10000_Passes)
{
    auto c = valid;
    c.max_connections = 10000;
    EXPECT_TRUE(model.is_valid(c));
}
TEST_F(ServerConfigTest, MaxConns_0_Fails)
{
    auto c = valid;
    c.max_connections = 0;
    EXPECT_FALSE(model.is_valid(c));
}
TEST_F(ServerConfigTest, MaxConns_10001_Fails)
{
    auto c = valid;
    c.max_connections = 10001;
    EXPECT_FALSE(model.is_valid(c));
}

TEST_F(ServerConfigTest, EmptyHost_Fails)
{
    auto c = valid;
    c.host = "";
    EXPECT_FALSE(model.is_valid(c));
}
TEST_F(ServerConfigTest, ZeroTimeout_Fails)
{
    auto c = valid;
    c.timeout_seconds = 0.0;
    EXPECT_FALSE(model.is_valid(c));
}
TEST_F(ServerConfigTest, NegativeTimeout_Fails)
{
    auto c = valid;
    c.timeout_seconds = -1.0;
    EXPECT_FALSE(model.is_valid(c));
}

TEST_F(ServerConfigTest, VerySmallPositiveTimeout_Passes)
{
    auto c = valid;
    c.timeout_seconds = std::nextafter(0.0, 1.0);
    EXPECT_TRUE(model.is_valid(c));
}

// ============================================================================
// 3. Product catalog — multiple numeric + string rules on one object
// ============================================================================

class ProductTest : public ::testing::Test {
protected:
    vd::basic_model<Product> model =
        vd::basic_model<Product> {}
            .with(vd::member(&Product::sku, vd::string_rules::non_empty()))
            .with(vd::member(&Product::name, vd::string_rules::non_empty()))
            .with(vd::predicate([](const Product& p) {
                return p.price > 0.0 && p.price <= 10000.0;
            }))
            .with(vd::member(&Product::stock, vd::int_bounds::inclusive(0, std::numeric_limits<std::int32_t>::max())))
            .with(vd::member(&Product::discount_percent, vd::double_bounds::inclusive(0.0, 100.0)));

    Product valid { "AB-1234", "Widget Pro", 49.99, 100, 10.0 };
};

TEST_F(ProductTest, ValidProduct_Passes)
{
    EXPECT_TRUE(model.is_valid(valid));
}
TEST_F(ProductTest, ZeroPrice_Fails)
{
    auto p = valid;
    p.price = 0.0;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, NegativePrice_Fails)
{
    auto p = valid;
    p.price = -0.01;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, PriceAt10000_Passes)
{
    auto p = valid;
    p.price = 10000.0;
    EXPECT_TRUE(model.is_valid(p));
}
TEST_F(ProductTest, PriceJustAbove10000_Fails)
{
    auto p = valid;
    p.price = 10000.01;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, ZeroStock_Passes)
{
    auto p = valid;
    p.stock = 0;
    EXPECT_TRUE(model.is_valid(p));
}
TEST_F(ProductTest, NegativeStock_Fails)
{
    auto p = valid;
    p.stock = -1;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, Discount0_Passes)
{
    auto p = valid;
    p.discount_percent = 0.0;
    EXPECT_TRUE(model.is_valid(p));
}
TEST_F(ProductTest, Discount100_Passes)
{
    auto p = valid;
    p.discount_percent = 100.0;
    EXPECT_TRUE(model.is_valid(p));
}
TEST_F(ProductTest, DiscountOver100_Fails)
{
    auto p = valid;
    p.discount_percent = 100.01;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, NegativeDiscount_Fails)
{
    auto p = valid;
    p.discount_percent = -0.01;
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, EmptySKU_Fails)
{
    auto p = valid;
    p.sku = "";
    EXPECT_FALSE(model.is_valid(p));
}
TEST_F(ProductTest, EmptyName_Fails)
{
    auto p = valid;
    p.name = "";
    EXPECT_FALSE(model.is_valid(p));
}

// ============================================================================
// 4. Cross-field validation via predicates
// ============================================================================

class TransferTest : public ::testing::Test {
protected:
    vd::basic_model<Transfer> model = vd::basic_model<Transfer> {}
                                          .with(vd::member(&Transfer::amount, vd::double_bounds::greater_than(0.0)))
                                          .with(vd::predicate([](const Transfer& t) {
                                              return t.fee >= 0.0;
                                          }))
                                          .with(vd::member(&Transfer::from_account, vd::string_rules::non_empty()))
                                          .with(vd::member(&Transfer::to_account, vd::string_rules::non_empty()))
                                          // Cross-field: fee must be strictly less than amount
                                          .with(vd::predicate([](const Transfer& t) {
                                              return t.fee < t.amount;
                                          }))
                                          // Cross-field: must not transfer to the same account
                                          .with(vd::predicate([](const Transfer& t) {
                                              return t.from_account != t.to_account;
                                          }));

    Transfer valid { 1000.0, 5.0, "ACC-001", "ACC-002" };
};

TEST_F(TransferTest, ValidTransfer_Passes)
{
    EXPECT_TRUE(model.is_valid(valid));
}
TEST_F(TransferTest, ZeroAmount_Fails)
{
    auto t = valid;
    t.amount = 0.0;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, NegativeAmount_Fails)
{
    auto t = valid;
    t.amount = -100.0;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, ZeroFee_Passes)
{
    auto t = valid;
    t.fee = 0.0;
    EXPECT_TRUE(model.is_valid(t));
}
TEST_F(TransferTest, FeeEqualToAmount_Fails)
{
    auto t = valid;
    t.fee = t.amount;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, FeeGreaterThanAmount_Fails)
{
    auto t = valid;
    t.fee = t.amount + 1.0;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, NegativeFee_Fails)
{
    auto t = valid;
    t.fee = -1.0;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, SelfTransfer_Fails)
{
    auto t = valid;
    t.to_account = t.from_account;
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, EmptyFromAccount_Fails)
{
    auto t = valid;
    t.from_account = "";
    EXPECT_FALSE(model.is_valid(t));
}
TEST_F(TransferTest, EmptyToAccount_Fails)
{
    auto t = valid;
    t.to_account = "";
    EXPECT_FALSE(model.is_valid(t));
}

TEST_F(TransferTest, FeeJustBelowAmount_Passes)
{
    auto t = valid;
    t.fee = std::nextafter(t.amount, 0.0);
    EXPECT_TRUE(model.is_valid(t));
}

// ============================================================================
// 5. Model composition: base model extended for a specific role
// ============================================================================

class ModelCompositionTest : public ::testing::Test {
protected:
    vd::basic_model<UserProfile> base_model = vd::basic_model<UserProfile> {}
                                                  .with(vd::member(&UserProfile::username, vd::string_rules::non_empty()))
                                                  .with(vd::member(&UserProfile::email, vd::string_rules::email_like()));

    vd::basic_model<UserProfile> admin_model =
        vd::basic_model<UserProfile> {}.with(base_model).with(vd::member(&UserProfile::admin_token, vd::string_rules::non_empty()));

    UserProfile regular { "bob", "bob@example.com", "Hello!", "" };
    UserProfile admin { "root", "root@corp.com", "Admin user", "secret-tok-xyz" };
};

TEST_F(ModelCompositionTest, BaseModel_AcceptsRegularUser)
{
    EXPECT_TRUE(base_model.is_valid(regular));
}
TEST_F(ModelCompositionTest, BaseModel_AcceptsAdminUser)
{
    EXPECT_TRUE(base_model.is_valid(admin));
}
TEST_F(ModelCompositionTest, AdminModel_AcceptsAdminUser)
{
    EXPECT_TRUE(admin_model.is_valid(admin));
}
TEST_F(ModelCompositionTest, AdminModel_RejectsEmptyToken)
{
    EXPECT_FALSE(admin_model.is_valid(regular));
}

TEST_F(ModelCompositionTest, AdminModel_StillEnforcesBaseRules)
{
    UserProfile bad_admin { "admin", "not-an-email", "bio", "valid-token" };
    EXPECT_FALSE(admin_model.is_valid(bad_admin));
}

TEST_F(ModelCompositionTest, WithModel_CombinesRulesFromBothSides)
{
    auto x_model = vd::basic_model<UserProfile> {}.with(vd::member(&UserProfile::username, vd::string_rules::non_empty()));
    auto y_model = vd::basic_model<UserProfile> {}.with(vd::member(&UserProfile::email, vd::string_rules::email_like()));

    auto combined = vd::basic_model<UserProfile> {}.with(x_model).with(y_model);

    EXPECT_TRUE(combined.is_valid(UserProfile { "alice", "alice@a.com", "", "" }));
    EXPECT_FALSE(combined.is_valid(UserProfile { "", "alice@a.com", "", "" }));
    EXPECT_FALSE(combined.is_valid(UserProfile { "alice", "bad-email", "", "" }));
    EXPECT_FALSE(combined.is_valid(UserProfile { "", "bad-email", "", "" }));
}

TEST_F(ModelCompositionTest, EmptyModel_ComposedWithAnother_OnlyOtherRulesApply)
{
    vd::basic_model<UserProfile> empty;
    auto combined = vd::basic_model<UserProfile> {}.with(empty).with(base_model);

    EXPECT_TRUE(combined.is_valid(regular));
    EXPECT_FALSE(combined.is_valid(UserProfile { "", "bad", "", "" }));
}

// ============================================================================
// 6. Bound model: non-obvious reference semantics
// ============================================================================

TEST(BoundModelReferenceSemantics, IsValid_ReflectsObjectMutation)
{
    // basic_bound_model stores const T* — it sees changes to the original object.
    std::int32_t counter = 5;
    auto model = vd::int_model {}.with(vd::rule<std::int32_t>(vd::int_bounds::inclusive(1, 10)));
    auto bound = model.bind(counter);

    EXPECT_TRUE(bound.is_valid());

    counter = 0;
    EXPECT_FALSE(bound.is_valid());

    counter = 10;
    EXPECT_TRUE(bound.is_valid());
}

TEST(BoundModelReferenceSemantics, BindByPointer_ReflectsMutation)
{
    std::int32_t value = 50;
    auto model = vd::int_model {}.with(vd::rule<std::int32_t>(vd::int_bounds::inclusive(0, 100)));
    auto bound = model.bind(&value);

    EXPECT_TRUE(bound.is_valid());
    value = 200;
    EXPECT_FALSE(bound.is_valid());
}

TEST(BoundModelReferenceSemantics, DeadCheck_AfterMutation_ThrowsWhenInvalid)
{
    std::int32_t value = 5;
    auto model = vd::int_model {}.with(vd::rule<std::int32_t>(vd::int_bounds::inclusive(1, 10)));
    auto bound = model.bind(value);

    EXPECT_NO_THROW(bound.die_if_failed());
    value = 0;
    EXPECT_THROW(bound.die_if_failed(), vd::validation_exception);
}

TEST(BoundModelReferenceSemantics, BindNull_Aborts)
{
    auto model = vd::int_model {};
    EXPECT_DEATH(model.bind(static_cast<const std::int32_t*>(nullptr)), "");
}

// ============================================================================
// 7. Bulk validation with standard algorithms
// ============================================================================

TEST(BulkValidationTest, CountValidProducts_InCatalog)
{
    auto model = vd::basic_model<Product> {}
                     .with(vd::predicate([](const Product& p) {
                         return p.price > 0.0;
                     }))
                     .with(vd::member(&Product::stock, vd::int_bounds::inclusive(0, std::numeric_limits<std::int32_t>::max())));

    std::vector<Product> catalog {
        { "A", "Alpha", 10.0, 5, 0.0 },  // valid
        { "B", "Beta", -1.0, 5, 0.0 },   // price invalid
        { "C", "Gamma", 5.0, -1, 0.0 },  // stock invalid
        { "D", "Delta", 20.0, 0, 0.0 },  // valid — zero stock is fine
        { "E", "Epsilon", 0.0, 1, 0.0 }, // price == 0, invalid
    };

    auto count = std::count_if(catalog.begin(), catalog.end(), [&model](const Product& p) {
        return model.is_valid(p);
    });

    EXPECT_EQ(count, 2);
}

TEST(BulkValidationTest, FilterValidRegistrations_IntoAccepted)
{
    auto model = make_registration_model();

    std::vector<UserRegistration> inputs {
        { "alice", "alice@ex.com", 25, "password1!" },     // valid
        { "", "bob@ex.com", 30, "password2!" },            // empty username
        { "charlie", "charlie@ex.com", 17, "password3!" }, // underage
        { "diana", "diana@ex.com", 28, "pass" },           // password too short
        { "eve", "eve@ex.com", 22, "securepass!" },        // valid
    };

    std::vector<UserRegistration> accepted;
    std::copy_if(inputs.begin(), inputs.end(), std::back_inserter(accepted), [&model](const UserRegistration& u) {
        return model.is_valid(u);
    });

    ASSERT_EQ(accepted.size(), 2u);
    EXPECT_EQ(accepted[0].username, "alice");
    EXPECT_EQ(accepted[1].username, "eve");
}

// ============================================================================
// 8. require_callback: per-field error collection
// ============================================================================

static std::vector<std::string> g_errors;
static void push_error(std::string_view msg)
{
    g_errors.push_back(std::move(std::string(msg.data(), msg.size())));
}

class FieldErrorCollectionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_errors.clear();
    }
};

TEST_F(FieldErrorCollectionTest, AllFieldsValid_NoErrors)
{
    UserRegistration good { "alice", "alice@example.com", 25, "securepass!" };

    vd::require_callback<push_error>(!good.username.empty(), "Username required");
    vd::require_callback<push_error>(vd::string_rules::email_like()(good.email), "Email invalid");
    vd::require_callback<push_error>(vd::int_bounds::inclusive(18, 120)(good.age), "Age out of range");
    vd::require_callback<push_error>(good.password.size() >= 8, "Password too short");

    EXPECT_TRUE(g_errors.empty());
}

TEST_F(FieldErrorCollectionTest, InvalidAge_CollectsFormattedMessage)
{
    UserRegistration bad { "alice", "alice@example.com", 15, "securepass!" };

    vd::require_callback<push_error>(!bad.username.empty(), "Username required");
    vd::require_callback<push_error>(vd::string_rules::email_like()(bad.email), "Email invalid");
    vd::require_callback<push_error>(vd::int_bounds::inclusive(18, 120)(bad.age), "Age {} is out of range [18, 120]", bad.age);
    vd::require_callback<push_error>(bad.password.size() >= 8, "Password too short");

    ASSERT_EQ(g_errors.size(), 1u);
    EXPECT_EQ(g_errors[0], "Age 15 is out of range [18, 120]");
}

TEST_F(FieldErrorCollectionTest, MultipleFieldsInvalid_AllErrorsCollected)
{
    UserRegistration bad { "", "bad-email", 15, "x" };

    vd::require_callback<push_error>(!bad.username.empty(), "Username required");
    vd::require_callback<push_error>(vd::string_rules::email_like()(bad.email), "Email invalid");
    vd::require_callback<push_error>(vd::int_bounds::inclusive(18, 120)(bad.age), "Age invalid");
    vd::require_callback<push_error>(bad.password.size() >= 8, "Password too short");

    EXPECT_EQ(g_errors.size(), 4u);
}

// ============================================================================
// 9. vd::require<ExceptionType> used as a guard in business logic
// ============================================================================

static bool start_server(const ServerConfig& cfg)
{
    auto model = make_server_config_model();
    vd::require<std::invalid_argument>(model.is_valid(cfg), "Invalid server config: host='{}', port={}", cfg.host, cfg.port);
    return true;
}

TEST(RequireInBusinessLogicTest, ValidConfig_ReturnsTrue)
{
    EXPECT_TRUE(start_server({ "0.0.0.0", 443, 100, 60.0 }));
}

TEST(RequireInBusinessLogicTest, InvalidPort_ThrowsInvalidArgument)
{
    EXPECT_THROW(start_server({ "localhost", 0, 100, 1.0 }), std::invalid_argument);
}

TEST(RequireInBusinessLogicTest, ThrowMessage_ContainsHostAndPort)
{
    try {
        start_server({ "", 99999, 1, 1.0 });
        FAIL() << "Expected std::invalid_argument";
    }
    catch(const std::invalid_argument& e) {
        std::string msg(e.what());
        EXPECT_NE(msg.find("host="), std::string::npos);
        EXPECT_NE(msg.find("port="), std::string::npos);
    }
}

// ============================================================================
// 10. Extreme / edge-case numeric values
// ============================================================================

TEST(ExtremeValuesTest, INT32_MAX_PassesUnboundedModel)
{
    auto model = vd::int_model {}.with(vd::rule<std::int32_t>(vd::int_bounds::unbounded()));
    EXPECT_TRUE(model.is_valid(std::numeric_limits<std::int32_t>::max()));
}

TEST(ExtremeValuesTest, INT32_MIN_PassesUnboundedModel)
{
    auto model = vd::int_model {}.with(vd::rule<std::int32_t>(vd::int_bounds::unbounded()));
    EXPECT_TRUE(model.is_valid(std::numeric_limits<std::int32_t>::min()));
}

TEST(ExtremeValuesTest, Infinity_FailsFiniteInclusiveBounds)
{
    auto bounds = vd::double_bounds::inclusive(-1e300, 1e300);
    EXPECT_FALSE(bounds(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(bounds(-std::numeric_limits<double>::infinity()));
}

TEST(ExtremeValuesTest, NaN_FailsAnyNumericBounds)
{
    // IEEE 754: all comparisons involving NaN return false, so NaN fails >= min && <= max.
    auto bounds = vd::double_bounds::inclusive(-1000.0, 1000.0);
    EXPECT_FALSE(bounds(std::numeric_limits<double>::quiet_NaN()));
}

TEST(ExtremeValuesTest, NaN_AndInfinity_DetectedByExplicitPredicate)
{
    auto model = vd::double_model {}.with(vd::rule<double>([](const double& v) {
        return !std::isnan(v) && !std::isinf(v) && v >= 0.0;
    }));

    EXPECT_FALSE(model.is_valid(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(model.is_valid(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(model.is_valid(-1.0));
    EXPECT_TRUE(model.is_valid(0.0));
    EXPECT_TRUE(model.is_valid(42.0));
}

TEST(ExtremeValuesTest, UINT8_AgeRange_MaxValueExcluded)
{
    auto bounds = vd::byte_bounds::inclusive(0, 120);
    EXPECT_TRUE(bounds(0));
    EXPECT_TRUE(bounds(120));
    EXPECT_FALSE(bounds(121));
    EXPECT_FALSE(bounds(255));
}

// ============================================================================
// 11. Regex-based string validation
// ============================================================================

TEST(RegexValidationTest, SKU_TwoUpperLettersDashFourDigits)
{
    auto checker = vd::string_rules::regex(R"([A-Z]{2}-\d{4})");

    EXPECT_TRUE(checker("AB-1234"));
    EXPECT_TRUE(checker("ZZ-0000"));
    EXPECT_FALSE(checker("ab-1234")); // lowercase
    EXPECT_FALSE(checker("A-1234"));  // only one letter
    EXPECT_FALSE(checker("AB-123"));  // three digits
    EXPECT_FALSE(checker("AB1234"));  // missing dash
    EXPECT_FALSE(checker(""));
}

TEST(RegexValidationTest, PhoneNumber_E164Like)
{
    auto checker = vd::string_rules::regex(R"(\+[1-9]\d{7,14})");

    EXPECT_TRUE(checker("+79001234567"));
    EXPECT_TRUE(checker("+14155552671"));
    EXPECT_FALSE(checker("79001234567")); // missing +
    EXPECT_FALSE(checker("+0123456789")); // country code starts with 0
    EXPECT_FALSE(checker("+1234"));       // too short
}

TEST(RegexValidationTest, RegexCheckerComposedInModel)
{
    struct Item {
        std::string code;
        double value;
    };

    auto model = vd::basic_model<Item> {}
                     .with(vd::member(&Item::code, vd::string_rules::regex(R"([A-Z]{2}-\d{4})")))
                     .with(vd::member(&Item::value, vd::double_bounds::greater_than(0.0)));

    EXPECT_TRUE(model.is_valid({ "AB-1234", 1.0 }));
    EXPECT_FALSE(model.is_valid({ "ab-1234", 1.0 })); // bad code
    EXPECT_FALSE(model.is_valid({ "AB-1234", 0.0 })); // bad value
    EXPECT_FALSE(model.is_valid({ "", -1.0 }));       // both fail
}

// ============================================================================
// 12. Dynamic model building: rules assembled at runtime
// ============================================================================

TEST(DynamicModelBuildingTest, RuntimeConstraints_Intersected)
{
    // Simulates loading validation rules from an external config.
    // All bounds are ANDed; the value must satisfy every constraint.
    struct Constraint {
        std::int32_t lo, hi;
    };
    std::vector<Constraint> constraints { { 0, 100 }, { 10, 50 }, { 25, 75 } };
    // Effective valid range: intersection = [25, 50]

    vd::int_model model;
    for(const auto& c : constraints)
        model.add_rule(vd::rule<std::int32_t>(vd::int_bounds::inclusive(c.lo, c.hi)));

    EXPECT_TRUE(model.is_valid(25));
    EXPECT_TRUE(model.is_valid(50));
    EXPECT_FALSE(model.is_valid(24)); // excluded by [25,75]
    EXPECT_FALSE(model.is_valid(51)); // excluded by [10,50]
    EXPECT_FALSE(model.is_valid(0));
    EXPECT_FALSE(model.is_valid(100));
}

TEST(DynamicModelBuildingTest, ModelExtendedAtRuntime_NewRuleEnforced)
{
    // Start with only the name rule, then add price validation later.
    auto model = vd::basic_model<Product> {}.with(vd::member(&Product::name, vd::string_rules::non_empty()));

    Product p { "X", "Widget", 5.0, 1, 0.0 };
    EXPECT_TRUE(model.is_valid(p));

    model.add_rule(vd::predicate([](const Product& pr) {
        return pr.price > 0.0 && pr.price <= 100.0;
    }));

    EXPECT_TRUE(model.is_valid(p)); // 5.0 in (0, 100]

    p.price = 200.0;
    EXPECT_FALSE(model.is_valid(p)); // 200 > 100

    p.price = 0.0;
    EXPECT_FALSE(model.is_valid(p)); // 0 not > 0
}

// ============================================================================
// 13. vd::result API — result object content inspection
//
// These tests verify that model.is_valid() returns a vd::result that carries
// not only a pass/fail flag but also diagnostic messages from the rules that
// failed. Rule message propagation depends on what the checker returns:
//
//  - Checker returns vd::result  (string_rules, numeric_bounds): the checker's
//    own message is forwarded into result.failed_rules.
//  - Checker returns bool (plain lambda): vd::member generates a fallback
//    message "Field '<name>' failed validation" using the supplied field name.
//
// vd::result also supports direct construction via result::ok() / result::failed()
// and exposes die_if_failed() + format() for pipeline-style error handling.
// ============================================================================

TEST(ResultObjectTest, Ok_IsValidTrue_NoFailedRules)
{
    auto r = vd::result::ok();
    EXPECT_TRUE(r.is_valid);
    EXPECT_TRUE(r.failed_rules.empty());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(ResultObjectTest, Failed_IsValidFalse_CarriesAllMessages)
{
    auto r = vd::result::failed({ "first error", "second error" });
    EXPECT_FALSE(r.is_valid);
    EXPECT_FALSE(static_cast<bool>(r));
    ASSERT_EQ(r.failed_rules.size(), 2u);
    EXPECT_EQ(r.failed_rules[0], "first error");
    EXPECT_EQ(r.failed_rules[1], "second error");
}

TEST(ResultObjectTest, DieIfFailed_OkResult_DoesNotThrow)
{
    EXPECT_NO_THROW(vd::result::ok().die_if_failed());
}

TEST(ResultObjectTest, DieIfFailed_FailedResult_ThrowsValidationException)
{
    auto r = vd::result::failed({ "something went wrong" });
    EXPECT_THROW(r.die_if_failed(), vd::validation_exception);
}

TEST(ResultObjectTest, DieIfFailed_ExceptionMessage_IsNonEmpty)
{
    auto r = vd::result::failed({ "some descriptive error" });
    try {
        r.die_if_failed();
        FAIL() << "Expected vd::validation_exception";
    }
    catch(const vd::validation_exception& ex) {
        EXPECT_NE(std::string(ex.what()), "");
    }
}

TEST(ResultObjectTest, Format_FailedResult_EachMessageAppearsInReport)
{
    auto r = vd::result::failed({ "field X is wrong", "value out of bounds" });
    std::string report = r.format();
    EXPECT_NE(report.find("field X is wrong"), std::string::npos);
    EXPECT_NE(report.find("value out of bounds"), std::string::npos);
}

TEST(ResultObjectTest, ModelPasses_Result_IsValidAndEmptyFailedRules)
{
    UserRegistration good { "alice", "alice@example.com", 25, "securepass!" };
    auto r = make_registration_model().is_valid(good);
    EXPECT_TRUE(r.is_valid);
    EXPECT_TRUE(r.failed_rules.empty());
}

// string_rules::non_empty() returns vd::result; vd::member propagates its
// message directly — the field name does NOT appear, the checker description does.
TEST(ResultObjectTest, NamedMemberStringRule_EmptyString_MessageIsCheckerDescription)
{
    auto model = vd::basic_model<UserRegistration> {}
                     .with(vd::member("username", &UserRegistration::username, vd::string_rules::non_empty()));

    auto r = model.is_valid(UserRegistration { "", "a@b.com", 25, "pass1234" });

    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_EQ(r.failed_rules[0], "string must be non-empty");
}

TEST(ResultObjectTest, NamedMemberEmailRule_InvalidEmail_MessageDescribesEmailFailure)
{
    auto model = vd::basic_model<UserRegistration> {}
                     .with(vd::member("email", &UserRegistration::email, vd::string_rules::email_like()));

    auto r = model.is_valid(UserRegistration { "alice", "notanemail", 25, "pass1234" });

    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_EQ(r.failed_rules[0], "string does not look like an email address");
}

// numeric_bounds returns vd::result; vd::member propagates it.
// Message includes the actual value and the configured bounds.
TEST(ResultObjectTest, UnnamedMemberNumericBounds_OutOfRange_MessageContainsValueAndBounds)
{
    auto model = vd::basic_model<UserRegistration> {}
                     .with(vd::member(&UserRegistration::age, vd::int_bounds::inclusive(18, 120)));

    auto r = model.is_valid(UserRegistration { "alice", "a@b.com", 15, "pass1234" });

    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_NE(r.failed_rules[0].find("15"), std::string::npos);
    EXPECT_NE(r.failed_rules[0].find("18"), std::string::npos);
    EXPECT_NE(r.failed_rules[0].find("120"), std::string::npos);
}

// When the checker returns plain bool, vd::member synthesises the message from
// the supplied field name: "Field '<name>' failed validation".
TEST(ResultObjectTest, NamedMemberBoolChecker_Fails_MessageContainsFieldName)
{
    auto model = vd::basic_model<UserRegistration> {}
                     .with(vd::member("password", &UserRegistration::password, [](const std::string& p) {
                         return p.size() >= 8;
                     }));

    auto r = model.is_valid(UserRegistration { "alice", "a@b.com", 25, "short" });

    EXPECT_FALSE(r.is_valid);
    ASSERT_EQ(r.failed_rules.size(), 1u);
    EXPECT_NE(r.failed_rules[0].find("password"), std::string::npos);
}

// Service-boundary use case: the caller uses result-as-bool to gate processing
// and can also inspect failed_rules to build a diagnostic response.
TEST(ResultObjectTest, ServiceBoundary_ResultInspection_GatesProcessingAndProvidesReport)
{
    auto model = make_server_config_model();
    ServerConfig bad { "", 0, 0, -1.0 };

    auto r = model.is_valid(bad);

    ASSERT_FALSE(r); // operator bool() gates further processing
    EXPECT_FALSE(r.failed_rules.empty()); // at least the first failure is captured
    EXPECT_FALSE(r.format().empty());     // human-readable report is available
}
