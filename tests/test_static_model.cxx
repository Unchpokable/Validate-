#include "gtest/gtest.h"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#define VD_EXPORT_UNSAFE
#include <vd.hxx>

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

struct Point {
    double x;
    double y;

    double get_x() const
    {
        return x;
    }
    double get_y() const
    {
        return y;
    }
    double magnitude() const
    {
        return x * x + y * y;
    }
};

struct Person {
    std::string name;
    int age;
};

struct Address {
    std::string city;
    std::string zip;
};

struct Contact {
    Person person;
    Address address;
};

// Deliberately heterogeneous in its arithmetic member types — used to show that
// a single vd::numeric::finite_t serves all of them.
struct Reading {
    double celsius;
    float humidity;
    std::int32_t samples;
};

// ---------------------------------------------------------------------------
// StaticModelEmptyTest
// A model with no rules is vacuously valid — it accepts any object.
// Consistent with the behaviour of basic_model when no rules are added.
// ---------------------------------------------------------------------------

TEST(StaticModelEmptyTest, EmptyModelAcceptsAnyObject)
{
    auto model = vd::make_static_model<Point>();
    EXPECT_TRUE(model.check(Point { -999.0, -999.0 }));
}

TEST(StaticModelEmptyTest, EmptyModelShortCheckAcceptsAnyObject)
{
    auto model = vd::make_static_model<Point>();
    EXPECT_TRUE(model.short_check(Point { 0.0, 0.0 }));
}

// nullptr is an invalid object regardless of rule set — even the empty model rejects it.
TEST(StaticModelEmptyTest, EmptyModelRejectsNullptr)
{
    auto model = vd::make_static_model<Point>();
    EXPECT_FALSE(model.check(static_cast<const Point*>(nullptr)));
}

// ---------------------------------------------------------------------------
// StaticModelSingleRuleTest
// Basic acceptance/rejection via each factory (member, field, predicate).
// ---------------------------------------------------------------------------

TEST(StaticModelSingleRuleTest, MemberRuleAcceptsValidObject)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(model.check(Point { 5.0, 0.0 }));
}

TEST(StaticModelSingleRuleTest, MemberRuleRejectsInvalidObject)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_FALSE(model.check(Point { -1.0, 0.0 }));
}

TEST(StaticModelSingleRuleTest, MemberRuleAcceptsBoundaryValues)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(model.check(Point { 0.0, 0.0 }));
    EXPECT_TRUE(model.check(Point { 10.0, 0.0 }));
}

TEST(StaticModelSingleRuleTest, FieldRuleWorksWithGetter)
{
    auto model = vd::make_static_model<Point>().with(vd::field(&Point::get_x, vd::double_bounds::greater_than(0.0)));
    EXPECT_TRUE(model.check(Point { 1.0, 0.0 }));
    EXPECT_FALSE(model.check(Point { 0.0, 0.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 0.0 }));
}

TEST(StaticModelSingleRuleTest, FieldRuleWorksWithComputedGetter)
{
    // magnitude() = x*x + y*y — verifies that any const member function works as a field getter.
    auto model = vd::make_static_model<Point>().with(vd::field(&Point::magnitude, vd::double_bounds::less_than(25.0)));
    EXPECT_TRUE(model.check(Point { 3.0, 0.0 }));  // magnitude = 9 < 25
    EXPECT_FALSE(model.check(Point { 3.0, 4.0 })); // magnitude = 25, less_than is exclusive
}

TEST(StaticModelSingleRuleTest, PredicateBoolLambdaAccepts)
{
    auto model = vd::make_static_model<Point>().with([](const Point& p) {
        return p.x > 0 && p.y > 0;
    });
    EXPECT_TRUE(model.check(Point { 1.0, 1.0 }));
}

TEST(StaticModelSingleRuleTest, PredicateBoolLambdaRejects)
{
    auto model = vd::make_static_model<Point>().with([](const Point& p) {
        return p.x > 0 && p.y > 0;
    });
    EXPECT_FALSE(model.check(Point { -1.0, 1.0 }));
    EXPECT_FALSE(model.check(Point { 1.0, -1.0 }));
}

TEST(StaticModelSingleRuleTest, PredicateResultLambdaAccepts)
{
    auto model = vd::make_static_model<Point>().with([](const Point& p) -> vd::result {
        return p.x >= 0 ? vd::result::ok() : vd::result::failed({ "x must be non-negative" });
    });
    EXPECT_TRUE(model.check(Point { 0.0, 0.0 }));
}

TEST(StaticModelSingleRuleTest, PredicateResultLambdaRejects)
{
    auto model = vd::make_static_model<Point>().with([](const Point& p) -> vd::result {
        return p.x >= 0 ? vd::result::ok() : vd::result::failed({ "x must be non-negative" });
    });
    EXPECT_FALSE(model.check(Point { -1.0, 0.0 }));
}

// ---------------------------------------------------------------------------
// StaticModelMultiRuleTest
// All rules must pass; each failing rule is independently tested.
// ---------------------------------------------------------------------------

TEST(StaticModelMultiRuleTest, AllRulesMustPassForValidResult)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.check(Point { 5.0, 5.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 5.0 }));  // only x fails
    EXPECT_FALSE(model.check(Point { 5.0, -1.0 }));  // only y fails
    EXPECT_FALSE(model.check(Point { -1.0, -1.0 })); // both fail
}

TEST(StaticModelMultiRuleTest, ThreeRulesAllMustPass)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::predicate([](const Point& p) {
                         return p.x != p.y;
                     }));

    EXPECT_TRUE(model.check(Point { 3.0, 4.0 }));
    EXPECT_FALSE(model.check(Point { 5.0, 5.0 }));  // third rule fails (x == y)
    EXPECT_FALSE(model.check(Point { -1.0, 5.0 })); // first rule fails
}

// ---------------------------------------------------------------------------
// StaticModelCheckVsShortCheckTest
// check() accumulates all failures; short_check() stops at the first one.
// These tests use side effects to verify evaluation order and call count.
// ---------------------------------------------------------------------------

TEST(StaticModelCheckVsShortCheckTest, CheckEvaluatesAllRulesEvenWhenFirstFails)
{
    int second_rule_calls = 0;

    auto model = vd::make_static_model<Point>()
                     .with([](const Point& p) {
                         return p.x > 0;
                     })
                     .with([&second_rule_calls](const Point& /*p*/) {
                         ++second_rule_calls;
                         return true;
                     });

    static_cast<void>(model.check(Point { -1.0, 0.0 })); // first rule fails; result intentionally discarded
    EXPECT_EQ(second_rule_calls, 1);                     // second rule was still called
}

TEST(StaticModelCheckVsShortCheckTest, ShortCheckDoesNotEvaluateRulesAfterFirstFailure)
{
    int second_rule_calls = 0;

    auto model = vd::make_static_model<Point>()
                     .with([](const Point& p) {
                         return p.x > 0;
                     })
                     .with([&second_rule_calls](const Point& /*p*/) {
                         ++second_rule_calls;
                         return true;
                     });

    static_cast<void>(model.short_check(Point { -1.0, 0.0 })); // first rule fails; result intentionally discarded
    EXPECT_EQ(second_rule_calls, 0);                           // second rule was not reached
}

TEST(StaticModelCheckVsShortCheckTest, CheckAccumulatesAllFailedRules)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.check(Point { -1.0, -1.0 }); // both x and y fail
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.failed_rules.size(), 2);        // both failures reported
}

TEST(StaticModelCheckVsShortCheckTest, ShortCheckReportsOnlyFirstFailure)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.short_check(Point { -1.0, -1.0 }); // both would fail
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.failed_rules.size(), 1);              // only first failure reported
}

TEST(StaticModelCheckVsShortCheckTest, BothReturnOkForValidObject)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.check(Point { 5.0, 5.0 }));
    EXPECT_TRUE(model.short_check(Point { 5.0, 5.0 }));
}

TEST(StaticModelCheckVsShortCheckTest, ShortCheckWithAllPassingRulesCallsAllOfThem)
{
    int total_calls = 0;

    auto model = vd::make_static_model<Point>()
                     .with([&total_calls](const Point& /*p*/) {
                         ++total_calls;
                         return true;
                     })
                     .with([&total_calls](const Point& /*p*/) {
                         ++total_calls;
                         return true;
                     });

    static_cast<void>(model.short_check(Point { 1.0, 1.0 })); // result intentionally discarded
    EXPECT_EQ(total_calls, 2);                                // all rules evaluated when none fail
}

// ---------------------------------------------------------------------------
// StaticModelPointerOverloadTest
// check(const T*) and short_check(const T*) — nullptr handling and delegation.
// ---------------------------------------------------------------------------

TEST(StaticModelPointerOverloadTest, NullptrCheckReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_FALSE(model.check(static_cast<const Point*>(nullptr)));
}

TEST(StaticModelPointerOverloadTest, NullptrShortCheckReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_FALSE(model.short_check(static_cast<const Point*>(nullptr)));
}

TEST(StaticModelPointerOverloadTest, ValidPointerPassesCheck)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point p { 5.0, 0.0 };
    EXPECT_TRUE(model.check(&p));
}

TEST(StaticModelPointerOverloadTest, InvalidPointerFailsCheck)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point p { -1.0, 0.0 };
    EXPECT_FALSE(model.check(&p));
}

// Pointer check and reference check must agree on the same object.
TEST(StaticModelPointerOverloadTest, PointerAndReferenceCheckAgree)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    Point valid { 5.0, 5.0 };
    Point invalid { -1.0, 5.0 };

    EXPECT_EQ(model.check(valid).is_valid, model.check(&valid).is_valid);
    EXPECT_EQ(model.check(invalid).is_valid, model.check(&invalid).is_valid);
}

// ---------------------------------------------------------------------------
// StaticModelDieIfFailedTest
// die_if_failed throws vd::validation_exception on invalid input.
// ---------------------------------------------------------------------------

TEST(StaticModelDieIfFailedTest, ValidObjectDoesNotThrow)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_NO_THROW(model.die_if_failed(Point { 5.0, 0.0 }));
}

TEST(StaticModelDieIfFailedTest, InvalidObjectThrowsValidationException)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_THROW(model.die_if_failed(Point { -1.0, 0.0 }), vd::validation_exception);
}

TEST(StaticModelDieIfFailedTest, ThrownExceptionIsCatchableAsStdException)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_THROW(model.die_if_failed(Point { -1.0, 0.0 }), std::exception);
}

TEST(StaticModelDieIfFailedTest, NullptrThrowsValidationException)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_THROW(model.die_if_failed(static_cast<const Point*>(nullptr)), vd::validation_exception);
}

TEST(StaticModelDieIfFailedTest, ValidPointerDoesNotThrow)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point p { 5.0, 0.0 };
    EXPECT_NO_THROW(model.die_if_failed(&p));
}

TEST(StaticModelDieIfFailedTest, InvalidPointerThrowsValidationException)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point p { -1.0, 0.0 };
    EXPECT_THROW(model.die_if_failed(&p), vd::validation_exception);
}

// ---------------------------------------------------------------------------
// StaticModelWithImmutabilityTest
// with() returns a new model — it must never modify the original.
// This is the defining structural difference from basic_model::with().
// ---------------------------------------------------------------------------

TEST(StaticModelWithImmutabilityTest, BaseModelUnchangedAfterExtension)
{
    auto base = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    // Extend base with a y-rule — must not touch base
    auto extended = base.with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    // base only validates x: a wildly invalid y must still pass base
    EXPECT_TRUE(base.check(Point { 5.0, -999.0 }));
    EXPECT_FALSE(base.check(Point { -1.0, 0.0 }));

    // extended validates both
    EXPECT_FALSE(extended.check(Point { 5.0, -999.0 }));
    EXPECT_TRUE(extended.check(Point { 5.0, 5.0 }));
}

TEST(StaticModelWithImmutabilityTest, TwoIndependentExtensionsFromSameBase)
{
    auto base = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto with_y = base.with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    auto with_pred = base.with(vd::predicate([](const Point& p) {
        return p.x != p.y;
    }));

    // base is unaffected
    EXPECT_TRUE(base.check(Point { 5.0, 5.0 }));

    // with_y rejects bad y, doesn't care about x==y
    EXPECT_FALSE(with_y.check(Point { 5.0, -1.0 }));
    EXPECT_TRUE(with_y.check(Point { 5.0, 5.0 }));

    // with_pred rejects x==y, doesn't care about y range
    EXPECT_FALSE(with_pred.check(Point { 5.0, 5.0 }));
    EXPECT_TRUE(with_pred.check(Point { 3.0, 4.0 }));
}

// ---------------------------------------------------------------------------
// StaticModelRawCallableTest
// Raw callables (lambdas, function objects) can be passed directly to with()
// without going through vd::rule<T> / std::function.
// ---------------------------------------------------------------------------

TEST(StaticModelRawCallableTest, BoolLambdaStoredAndInvokedCorrectly)
{
    // No factory — lambda goes directly into the tuple
    auto model = vd::make_static_model<Point>().with([](const Point& p) {
        return p.x > 0 && p.y > 0;
    });

    EXPECT_TRUE(model.check(Point { 1.0, 1.0 }));
    EXPECT_FALSE(model.check(Point { 0.0, 1.0 }));
}

TEST(StaticModelRawCallableTest, BoolLambdaFailureProducesNonEmptyMessage)
{
    auto model = vd::make_static_model<Point>().with([](const Point& p) {
        return p.x > 0;
    });

    auto result = model.check(Point { -1.0, 0.0 });
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.failed_rules.empty());
}

TEST(StaticModelRawCallableTest, ResultLambdaFailurePreservesMessage)
{
    const std::string expected_msg = "x must be strictly positive";

    auto model = vd::make_static_model<Point>().with([&](const Point& p) -> vd::result {
        return p.x > 0 ? vd::result::ok() : vd::result::failed({ expected_msg });
    });

    auto result = model.check(Point { -1.0, 0.0 });
    EXPECT_FALSE(result.is_valid);
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_EQ(result.failed_rules[0], expected_msg);
}

TEST(StaticModelRawCallableTest, FactoryRuleAndRawLambdaComposeCorrectly)
{
    // vd::member produces rule<Point> (type-erased via std::function).
    // The raw lambda is a distinct type — both live in the same tuple.
    auto model =
        vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0))).with([](const Point& p) {
            return p.y >= 0.0;
        });

    EXPECT_TRUE(model.check(Point { 5.0, 1.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 1.0 }));  // factory rule fails
    EXPECT_FALSE(model.check(Point { 5.0, -1.0 }));  // lambda fails
    EXPECT_FALSE(model.check(Point { -1.0, -1.0 })); // both fail
}

TEST(StaticModelRawCallableTest, AllThreeFactoriesComposeWithRawLambda)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
                     .with(vd::field(&Point::magnitude, vd::double_bounds::less_than(10000.0)))
                     .with(vd::predicate([](const Point& p) {
                         return !std::isnan(p.x) && !std::isnan(p.y);
                     }))
                     .with([](const Point& p) {
                         return p.x != p.y;
                     });

    EXPECT_TRUE(model.check(Point { 3.0, 4.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 4.0 }));  // x out of range
    EXPECT_FALSE(model.check(Point { 99.0, 99.0 })); // x == y fails last rule
    EXPECT_FALSE(model.check(Point { std::numeric_limits<double>::quiet_NaN(), 0.0 }));
}

// ---------------------------------------------------------------------------
// StaticModelResultContentTest
// The content of vd::result must reflect the actual failing rule.
// ---------------------------------------------------------------------------

TEST(StaticModelResultContentTest, SuccessfulCheckReturnsValidResult)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.check(Point { 5.0, 0.0 });
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.failed_rules.empty());
}

TEST(StaticModelResultContentTest, FailedResultContainsFieldNameFromFactory)
{
    auto model = vd::make_static_model<Point>().with(vd::member("x-coordinate", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.check(Point { -1.0, 0.0 });
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_NE(result.failed_rules[0].find("x-coordinate"), std::string::npos);
}

TEST(StaticModelResultContentTest, ShortCheckPreservesFailureMessageFromFirstRule)
{
    // short_check must carry the error details from the first failing rule, not discard them.
    auto model = vd::make_static_model<Point>()
                     .with(vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.short_check(Point { -1.0, -1.0 });
    EXPECT_FALSE(result.is_valid);
    EXPECT_FALSE(result.failed_rules.empty());
    EXPECT_NE(result.failed_rules[0].find("x"), std::string::npos); // first rule (x) is reported
}

// ---------------------------------------------------------------------------
// StaticModelNestedTest
// Composing a static_model into a predicate to validate nested sub-objects.
// This is a key real-world usage pattern.
// ---------------------------------------------------------------------------

TEST(StaticModelNestedTest, NestedStaticModelValidation)
{
    auto address_model = vd::make_static_model<Address>()
                             .with(vd::member(&Address::city, vd::string_rules::non_empty()))
                             .with(vd::member(&Address::zip, vd::string_rules::non_empty()));

    // address_model captured by value — static_model is a value type
    auto contact_model = vd::make_static_model<Contact>()
                             .with(vd::member(&Contact::person,
                                 [address_model](const Person& /*p*/) {
                                     return true;
                                 }))
                             .with([address_model](const Contact& c) {
                                 return address_model.short_check(c.address);
                             });

    EXPECT_TRUE(contact_model.check(Contact { { "Alice", 30 }, { "NYC", "10001" } }));
    EXPECT_FALSE(contact_model.check(Contact { { "Bob", 25 }, { "", "10001" } })); // empty city
    EXPECT_FALSE(contact_model.check(Contact { { "Carol", 40 }, { "LA", "" } }));  // empty zip
}

TEST(StaticModelNestedTest, NestedModelCapturedByValueIsIndependentCopy)
{
    // After capturing, changes to the original address_model (or its going out of scope)
    // must not affect contact_model. Here we verify the captured copy validates correctly.
    auto make_address_model = [&]() {
        return vd::make_static_model<Address>().with(vd::member(&Address::city, vd::string_rules::non_empty()));
    };

    // contact_model captures a *copy* of the inner model created in the lambda above
    auto contact_model = vd::make_static_model<Contact>().with([inner = make_address_model()](const Contact& c) {
        return inner.check(c.address);
    });

    EXPECT_TRUE(contact_model.check(Contact { { "Alice", 30 }, { "NYC", "" } }));
    EXPECT_FALSE(contact_model.check(Contact { { "Alice", 30 }, { "", "" } })); // empty city
}

// ---------------------------------------------------------------------------
// StaticModelStringRulesTest
// Validates string fields through the standard vd::string_rules checkers.
// ---------------------------------------------------------------------------

TEST(StaticModelStringRulesTest, NonEmptyNameAccepted)
{
    auto model = vd::make_static_model<Person>().with(vd::member(&Person::name, vd::string_rules::non_empty()));
    EXPECT_TRUE(model.check(Person { "Alice", 30 }));
}

TEST(StaticModelStringRulesTest, EmptyNameRejected)
{
    auto model = vd::make_static_model<Person>().with(vd::member(&Person::name, vd::string_rules::non_empty()));
    EXPECT_FALSE(model.check(Person { "", 30 }));
}

TEST(StaticModelStringRulesTest, StringAndNumericRulesCombined)
{
    auto model = vd::make_static_model<Person>()
                     .with(vd::member(&Person::name, vd::string_rules::non_empty()))
                     .with(vd::member(&Person::age, vd::int_bounds::inclusive(0, 150)));

    EXPECT_TRUE(model.check(Person { "Alice", 30 }));
    EXPECT_FALSE(model.check(Person { "", 30 }));
    EXPECT_FALSE(model.check(Person { "Alice", -1 }));
    EXPECT_FALSE(model.check(Person { "", -1 }));
}

// ---------------------------------------------------------------------------
// StaticModelValidateManyTest
// validate_many free functions — all overloads.
// ---------------------------------------------------------------------------

TEST(StaticModelValidateManyTest, VariadicAllValidReturnsTrue)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { 5.0, 0.0 }, Point { 10.0, 0.0 }));
}

TEST(StaticModelValidateManyTest, VariadicOneInvalidReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_FALSE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { -1.0, 0.0 }, Point { 5.0, 0.0 }));
}

TEST(StaticModelValidateManyTest, VectorEmptyReturnsTrue)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(vd::validate_many(model, std::vector<Point> {}));
}

TEST(StaticModelValidateManyTest, VectorAllValidReturnsTrue)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    std::vector<Point> pts { { 0.0, 0.0 }, { 5.0, 0.0 }, { 10.0, 0.0 } };
    EXPECT_TRUE(vd::validate_many(model, pts));
}

TEST(StaticModelValidateManyTest, VectorOneInvalidReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    std::vector<Point> pts { { 1.0, 0.0 }, { -1.0, 0.0 }, { 5.0, 0.0 } };
    EXPECT_FALSE(vd::validate_many(model, pts));
}

TEST(StaticModelValidateManyTest, PtrVectorAllValidReturnsTrue)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point a { 1.0, 0.0 }, b { 5.0, 0.0 };
    std::vector<Point*> ptrs { &a, &b };
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(StaticModelValidateManyTest, PtrVectorOneInvalidReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point a { 1.0, 0.0 }, b { -1.0, 0.0 };
    std::vector<Point*> ptrs { &a, &b };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

TEST(StaticModelValidateManyTest, PtrVectorNullptrReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    Point a { 5.0, 0.0 };
    std::vector<Point*> ptrs { &a, nullptr };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

TEST(StaticModelValidateManyTest, ConstPtrVectorAllValidReturnsTrue)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    const Point a { 2.0, 0.0 }, b { 8.0, 0.0 };
    std::vector<const Point*> ptrs { &a, &b };
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(StaticModelValidateManyTest, ConstPtrVectorNullptrReturnsFalse)
{
    auto model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    const Point a { 5.0, 0.0 };
    std::vector<const Point*> ptrs { &a, nullptr };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

// ---------------------------------------------------------------------------
// StaticModelStaticsFactoryTest
// vd::statics::field / vd::statics::member return raw lambdas directly
// (deduced via `auto`), bypassing vd::rule<T> construction and the
// std::function allocation it implies. These are the factories meant to be
// paired with static_model for a fully heap-alloc-free pipeline.
// ---------------------------------------------------------------------------

TEST(StaticModelStaticsFactoryTest, StaticsMemberDoesNotProduceRuleType)
{
    auto rule_obj = vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0));
    auto static_rule = vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0));

    static_assert(std::same_as<decltype(rule_obj), vd::rule<Point>>);
    static_assert(!std::same_as<decltype(static_rule), vd::rule<Point>>);
}

TEST(StaticModelStaticsFactoryTest, StaticsFieldDoesNotProduceRuleType)
{
    auto rule_obj = vd::field(&Point::get_x, vd::double_bounds::greater_than(0.0));
    auto static_rule = vd::statics::field(&Point::get_x, vd::double_bounds::greater_than(0.0));

    static_assert(std::same_as<decltype(rule_obj), vd::rule<Point>>);
    static_assert(!std::same_as<decltype(static_rule), vd::rule<Point>>);
}

TEST(StaticModelStaticsFactoryTest, MemberFactoryAcceptsValidObject)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(model.check(Point { 5.0, 0.0 }));
}

TEST(StaticModelStaticsFactoryTest, MemberFactoryRejectsInvalidObject)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_FALSE(model.check(Point { -1.0, 0.0 }));
}

TEST(StaticModelStaticsFactoryTest, MemberFactoryAcceptsBoundaryValues)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_TRUE(model.check(Point { 0.0, 0.0 }));
    EXPECT_TRUE(model.check(Point { 10.0, 0.0 }));
}

TEST(StaticModelStaticsFactoryTest, FieldFactoryWorksWithGetter)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::field(&Point::get_x, vd::double_bounds::greater_than(0.0)));
    EXPECT_TRUE(model.check(Point { 1.0, 0.0 }));
    EXPECT_FALSE(model.check(Point { 0.0, 0.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 0.0 }));
}

TEST(StaticModelStaticsFactoryTest, FieldFactoryWorksWithComputedGetter)
{
    // magnitude() = x*x + y*y — same computed-getter scenario as the vd::field test, via vd::statics::field.
    auto model = vd::make_static_model<Point>().with(vd::statics::field(&Point::magnitude, vd::double_bounds::less_than(25.0)));
    EXPECT_TRUE(model.check(Point { 3.0, 0.0 }));  // magnitude = 9 < 25
    EXPECT_FALSE(model.check(Point { 3.0, 4.0 })); // magnitude = 25, less_than is exclusive
}

TEST(StaticModelStaticsFactoryTest, NamedMemberFailureMessageContainsFieldName)
{
    auto model =
        vd::make_static_model<Point>().with(vd::statics::member("x-coordinate", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.check(Point { -1.0, 0.0 });
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_NE(result.failed_rules[0].find("x-coordinate"), std::string::npos);
}

TEST(StaticModelStaticsFactoryTest, UnnamedMemberFailureMessageContainsUnnamed)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto result = model.check(Point { -1.0, 0.0 });
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_NE(result.failed_rules[0].find("Unnamed"), std::string::npos);
}

TEST(StaticModelStaticsFactoryTest, NamedFieldFailureMessageContainsFieldName)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::field("x-getter", &Point::get_x, vd::double_bounds::greater_than(0.0)));

    auto result = model.check(Point { -1.0, 0.0 });
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_NE(result.failed_rules[0].find("x-getter"), std::string::npos);
}

TEST(StaticModelStaticsFactoryTest, UnnamedFieldFailureMessageContainsUnnamed)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::field(&Point::get_x, vd::double_bounds::greater_than(0.0)));

    auto result = model.check(Point { -1.0, 0.0 });
    ASSERT_EQ(result.failed_rules.size(), 1);
    EXPECT_NE(result.failed_rules[0].find("Unnamed"), std::string::npos);
}

TEST(StaticModelStaticsFactoryTest, MultipleStaticsRulesAllMustPass)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::statics::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::statics::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.check(Point { 5.0, 5.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 5.0 }));  // only x fails
    EXPECT_FALSE(model.check(Point { 5.0, -1.0 }));  // only y fails

    auto result = model.check(Point { -1.0, -1.0 }); // both fail
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.failed_rules.size(), 2);
}

TEST(StaticModelStaticsFactoryTest, StaticsRuleComposesWithFactoryRuleAndRawLambda)
{
    // Three different "rule kinds" living in the same tuple side by side:
    //   - vd::member          -> rule<Point> (std::function, heap-allocating)
    //   - vd::statics::member -> raw lambda (no rule<T>, no std::function)
    //   - bare lambda         -> raw lambda, no factory at all
    auto model = vd::make_static_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::statics::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with([](const Point& p) {
                         return p.x != p.y;
                     });

    EXPECT_TRUE(model.check(Point { 3.0, 4.0 }));
    EXPECT_FALSE(model.check(Point { -1.0, 4.0 })); // first rule (vd::member) fails
    EXPECT_FALSE(model.check(Point { 5.0, -1.0 })); // second rule (vd::statics::member) fails
    EXPECT_FALSE(model.check(Point { 5.0, 5.0 }));  // third rule (raw lambda) fails
}

TEST(StaticModelStaticsFactoryTest, ShortCheckStopsAfterFirstStaticsRuleFailureAndKeepsItsMessage)
{
    int second_calls = 0;

    auto model = vd::make_static_model<Point>()
                     .with(vd::statics::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with([&second_calls](const Point& p) {
                         ++second_calls;
                         return p.y >= 0.0;
                     });

    auto result = model.short_check(Point { -1.0, -1.0 }); // first rule fails; second would also fail
    EXPECT_EQ(second_calls, 0);                            // short_check stopped before reaching it
    EXPECT_FALSE(result.is_valid);
    ASSERT_FALSE(result.failed_rules.empty());
    EXPECT_NE(result.failed_rules[0].find("x"), std::string::npos);
}

TEST(StaticModelStaticsFactoryTest, DieIfFailedThrowsForStaticsRuleFailure)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    EXPECT_NO_THROW(model.die_if_failed(Point { 5.0, 0.0 }));
    EXPECT_THROW(model.die_if_failed(Point { -1.0, 0.0 }), vd::validation_exception);
}

TEST(StaticModelStaticsFactoryTest, ValidateManyWithStaticsBasedModel)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { 5.0, 0.0 }, Point { 10.0, 0.0 }));
    EXPECT_FALSE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { -1.0, 0.0 }));
}

TEST(StaticModelStaticsFactoryTest, FieldAndMemberStaticsFactoriesMatchRuleVersionsFunctionally)
{
    // Both pipelines must agree on validity for every input — only the
    // allocation strategy differs, never the validation outcome.
    auto rule_model = vd::make_static_model<Point>()
                          .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                          .with(vd::field(&Point::get_y, vd::double_bounds::greater_than(0.0)));

    auto raw_model = vd::make_static_model<Point>()
                         .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                         .with(vd::statics::field(&Point::get_y, vd::double_bounds::greater_than(0.0)));

    for(const Point& p : { Point { 5.0, 1.0 }, Point { -1.0, 1.0 }, Point { 5.0, -1.0 }, Point { -1.0, -1.0 } }) {
        EXPECT_EQ(rule_model.check(p).is_valid, raw_model.check(p).is_valid);
    }
}

TEST(StaticModelStaticsFactoryTest, WithExtensionUsingStaticsFactoryPreservesBaseModel)
{
    auto base = vd::make_static_model<Point>().with(vd::statics::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)));
    auto extended = base.with(vd::statics::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    // base only checks x — a wildly invalid y must still pass it
    EXPECT_TRUE(base.check(Point { 5.0, -999.0 }));
    EXPECT_FALSE(base.check(Point { -1.0, 0.0 }));

    // extended checks both
    EXPECT_FALSE(extended.check(Point { 5.0, -999.0 }));
    EXPECT_TRUE(extended.check(Point { 5.0, 5.0 }));
}

// ---------------------------------------------------------------------------
// StaticModelFiniteTest — vd::numeric::finite_t inside static_model<T, Rules...>
//
// vd::numeric::finite_t is an inline constexpr finite_guard — an empty type
// whose operator() is the template, so the guard is not tied to any one value
// type. static_model stores it inline in its rule tuple, which pins down three
// things worth testing: no heap indirection, a constexpr-constructible model,
// and one guard object usable across differently typed members.
// ---------------------------------------------------------------------------

TEST(StaticModelFiniteTest, FiniteSatisfiesStaticRuleFor)
{
    using guard_t = vd::numeric::finite_guard;

    static_assert(vd::static_rule_for<guard_t, double>);
    static_assert(vd::static_rule_for<guard_t, float>);
    static_assert(vd::static_rule_for<guard_t, std::int32_t>);
    static_assert(std::is_same_v<std::invoke_result_t<guard_t, const double&>, vd::result>);

    // constrained on operator(), so non-arithmetic T merely fails the concept
    static_assert(!vd::static_rule_for<guard_t, std::string>);
    SUCCEED();
}

TEST(StaticModelFiniteTest, TopLevelRuleAcceptsFiniteValue)
{
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);

    EXPECT_TRUE(model.check(0.0));
    EXPECT_TRUE(model.check(-1.5));
    EXPECT_TRUE(model.check(std::numeric_limits<double>::max()));
}

TEST(StaticModelFiniteTest, TopLevelRuleRejectsInfinity)
{
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);

    EXPECT_FALSE(model.check(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(model.check(-std::numeric_limits<double>::infinity()));
}

TEST(StaticModelFiniteTest, TopLevelRuleRejectsNaN)
{
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
    EXPECT_FALSE(model.check(std::numeric_limits<double>::quiet_NaN()));
}

TEST(StaticModelFiniteTest, TopLevelRuleRejectsNullptr)
{
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
    EXPECT_FALSE(model.check(static_cast<const double*>(nullptr)));
}

TEST(StaticModelFiniteTest, TopLevelRuleFailureCarriesGuardMessage)
{
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
    vd::result res = model.check(std::numeric_limits<double>::quiet_NaN());

    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("not finite"), std::string::npos);
}

TEST(StaticModelFiniteTest, ModelIsConstexprConstructible)
{
    // finite_guard is an empty literal type and finite_t is already a constant,
    // so the whole model can be built in a constant expression. Only check()
    // runs at runtime — vd::result owns a std::vector and cannot be
    // constant-evaluated.
    constexpr auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);

    EXPECT_TRUE(model.check(1.0));
    EXPECT_FALSE(model.check(std::numeric_limits<double>::quiet_NaN()));
}

TEST(StaticModelFiniteTest, ModelStoresNoIndirection)
{
    // A stateless guard must not inflate the model — evidence that static_model
    // keeps the rule inline instead of behind a std::function.
    auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
    static_assert(sizeof(model) == 1);
    SUCCEED();
}

TEST(StaticModelFiniteTest, SameGuardBehavesTheSameAtFloatPrecision)
{
    auto model = vd::make_static_model<float>().with(vd::numeric::finite_t);

    EXPECT_TRUE(model.check(1.0f));
    EXPECT_TRUE(model.check(std::numeric_limits<float>::denorm_min()));
    EXPECT_FALSE(model.check(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(model.check(std::numeric_limits<float>::quiet_NaN()));
}

TEST(StaticModelFiniteTest, SameGuardAlwaysPassesIntegralValues)
{
    auto model = vd::make_static_model<std::int32_t>().with(vd::numeric::finite_t);

    EXPECT_TRUE(model.check(0));
    EXPECT_TRUE(model.check(std::numeric_limits<std::int32_t>::max()));
    EXPECT_TRUE(model.check(std::numeric_limits<std::int32_t>::lowest()));
}

TEST(StaticModelFiniteTest, OneGuardServesHeterogeneousMembers)
{
    // The payoff of templating operator() instead of the class: the same object
    // is handed to a double, a float and an int32 member of one model, and each
    // rule instantiates operator() at its own member type.
    auto model = vd::make_static_model<Reading>()
                     .with(vd::statics::member("celsius", &Reading::celsius, vd::numeric::finite_t))
                     .with(vd::statics::member("humidity", &Reading::humidity, vd::numeric::finite_t))
                     .with(vd::statics::member("samples", &Reading::samples, vd::numeric::finite_t));

    EXPECT_TRUE(model.check(Reading { 20.0, 0.5f, 10 }));
    EXPECT_TRUE(model.check(Reading { 20.0, 0.5f, std::numeric_limits<std::int32_t>::max() }));

    EXPECT_FALSE(model.check(Reading { std::numeric_limits<double>::quiet_NaN(), 0.5f, 10 }));
    EXPECT_FALSE(model.check(Reading { 20.0, std::numeric_limits<float>::infinity(), 10 }));
}

TEST(StaticModelFiniteTest, HeterogeneousFailuresReportTheirOwnMemberNames)
{
    auto model = vd::make_static_model<Reading>()
                     .with(vd::statics::member("celsius", &Reading::celsius, vd::numeric::finite_t))
                     .with(vd::statics::member("humidity", &Reading::humidity, vd::numeric::finite_t));

    vd::result res = model.check(Reading { std::numeric_limits<double>::infinity(), std::numeric_limits<float>::quiet_NaN(), 0 });

    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 2u);
    EXPECT_NE(res.failed_rules[0].find("celsius"), std::string::npos);
    EXPECT_NE(res.failed_rules[1].find("humidity"), std::string::npos);
}

TEST(StaticModelFiniteTest, StaticsMemberCheckerAcceptsFiniteValue)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::numeric::finite_t));

    EXPECT_TRUE(model.check(Point { 1.0, 0.0 }));
    EXPECT_FALSE(model.check(Point { std::numeric_limits<double>::quiet_NaN(), 0.0 }));
}

TEST(StaticModelFiniteTest, StaticsFieldCheckerViaGetter)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::field("get_y", &Point::get_y, vd::numeric::finite_t));

    EXPECT_TRUE(model.check(Point { 0.0, 1.0 }));
    EXPECT_FALSE(model.check(Point { 0.0, std::numeric_limits<double>::infinity() }));
}

TEST(StaticModelFiniteTest, RuleFactoryAndStaticsFactoryAgree)
{
    // vd::member wraps the guard in a vd::rule<T>; vd::statics::member keeps it
    // allocation-free. Only the storage strategy differs, never the verdict.
    auto rule_model = vd::make_static_model<Point>().with(vd::member(&Point::x, vd::numeric::finite_t));
    auto raw_model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::numeric::finite_t));

    for(const Point& p : { Point { 1.0, 0.0 },
            Point { std::numeric_limits<double>::quiet_NaN(), 0.0 },
            Point { std::numeric_limits<double>::infinity(), 0.0 } }) {
        EXPECT_EQ(rule_model.check(p).is_valid, raw_model.check(p).is_valid);
    }
}

TEST(StaticModelFiniteTest, CheckAggregatesEveryNonFiniteMember)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::statics::member("x", &Point::x, vd::numeric::finite_t))
                     .with(vd::statics::member("y", &Point::y, vd::numeric::finite_t));

    vd::result res = model.check(Point { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() });

    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.failed_rules.size(), 2u);
}

TEST(StaticModelFiniteTest, ShortCheckStopsAtFirstNonFiniteMember)
{
    auto model = vd::make_static_model<Point>()
                     .with(vd::statics::member("x", &Point::x, vd::numeric::finite_t))
                     .with(vd::statics::member("y", &Point::y, vd::numeric::finite_t));

    vd::result res = model.short_check(Point { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() });

    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("x"), std::string::npos);
}

TEST(StaticModelFiniteTest, MixesWithBoundsRules)
{
    // finite screens out NaN/inf before the bounds rule interprets them; NaN
    // fails both because every comparison against it is false.
    auto model = vd::make_static_model<Point>()
                     .with(vd::statics::member("x", &Point::x, vd::numeric::finite_t))
                     .with(vd::statics::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.check(Point { 5.0, 0.0 }));
    EXPECT_EQ(model.check(Point { std::numeric_limits<double>::quiet_NaN(), 0.0 }).failed_rules.size(), 2u);
    EXPECT_EQ(model.check(Point { 50.0, 0.0 }).failed_rules.size(), 1u);
}

TEST(StaticModelFiniteTest, DieIfFailedThrowsOnNonFiniteValue)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::numeric::finite_t));

    EXPECT_NO_THROW(model.die_if_failed(Point { 1.0, 0.0 }));
    EXPECT_THROW(model.die_if_failed(Point { std::numeric_limits<double>::infinity(), 0.0 }), vd::validation_exception);
}

TEST(StaticModelFiniteTest, ValidateManyRejectsBatchWithNonFiniteMember)
{
    auto model = vd::make_static_model<Point>().with(vd::statics::member(&Point::x, vd::numeric::finite_t));

    EXPECT_TRUE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { 2.0, 0.0 }));
    EXPECT_FALSE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { std::numeric_limits<double>::quiet_NaN(), 0.0 }));
}

TEST(StaticModelFiniteTest, WithExtensionPreservesBaseModel)
{
    auto base = vd::make_static_model<Point>().with(vd::statics::member("x", &Point::x, vd::numeric::finite_t));
    auto extended = base.with(vd::statics::member("y", &Point::y, vd::numeric::finite_t));

    // base only screens x — a non-finite y must still pass it
    EXPECT_TRUE(base.check(Point { 1.0, std::numeric_limits<double>::infinity() }));
    EXPECT_FALSE(extended.check(Point { 1.0, std::numeric_limits<double>::infinity() }));
    EXPECT_TRUE(extended.check(Point { 1.0, 2.0 }));
}
