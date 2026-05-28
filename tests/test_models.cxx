#include <gtest/gtest.h>
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

struct NamedValue {
    std::string name;
    int value;

    const std::string& get_name() const
    {
        return name;
    }
    int get_value() const
    {
        return value;
    }
};

// ---------------------------------------------------------------------------
// numeric_bounds — operator()
// Checks that bounds correctly accept and reject values at boundaries.
// ---------------------------------------------------------------------------

TEST(NumericBoundsTest, InclusiveAcceptsMinAndMax)
{
    auto bounds = vd::int_bounds::inclusive(0, 10);
    EXPECT_TRUE(bounds(0));
    EXPECT_TRUE(bounds(10));
    EXPECT_TRUE(bounds(5));
}

TEST(NumericBoundsTest, InclusiveRejectsOutside)
{
    auto bounds = vd::int_bounds::inclusive(0, 10);
    EXPECT_FALSE(bounds(-1));
    EXPECT_FALSE(bounds(11));
}

TEST(NumericBoundsTest, ExclusiveRejectsBoundaries)
{
    // exclusive(0.0, 1.0) should reject exactly 0.0 and 1.0
    auto bounds = vd::double_bounds::exclusive(0.0, 1.0);
    EXPECT_FALSE(bounds(0.0));
    EXPECT_FALSE(bounds(1.0));
    EXPECT_TRUE(bounds(0.5));
}

TEST(NumericBoundsTest, GreaterThanRejectsEqualAndBelow)
{
    auto bounds = vd::double_bounds::greater_than(5.0);
    EXPECT_FALSE(bounds(5.0));
    EXPECT_FALSE(bounds(4.9));
    EXPECT_TRUE(bounds(5.0 + 1e-10));
}

TEST(NumericBoundsTest, LessThanRejectsEqualAndAbove)
{
    auto bounds = vd::double_bounds::less_than(5.0);
    EXPECT_FALSE(bounds(5.0));
    EXPECT_FALSE(bounds(5.1));
    EXPECT_TRUE(bounds(5.0 - 1e-10));
}

TEST(NumericBoundsTest, UnboundedAcceptsAnyValue)
{
    auto bounds = vd::double_bounds::unbounded();
    EXPECT_TRUE(bounds(std::numeric_limits<double>::lowest()));
    EXPECT_TRUE(bounds(std::numeric_limits<double>::max()));
    EXPECT_TRUE(bounds(0.0));
}

// ---------------------------------------------------------------------------
// numeric_bounds — integer exclusive/greater_than/less_than
// std::nextafter is float-only; for integral T the old code silently produced
// inclusive semantics (nextafter(0, INT_MAX) -> 5e-324 -> truncated back to 0).
// ct_nextafter steps by ±1 for integers, so these boundaries are now correct.
// ---------------------------------------------------------------------------

TEST(NumericBoundsTest, IntegerExclusiveRejectsBoundariesAcceptsInside)
{
    auto bounds = vd::int_bounds::exclusive(0, 10);
    EXPECT_FALSE(bounds(0));
    EXPECT_FALSE(bounds(10));
    EXPECT_TRUE(bounds(1));
    EXPECT_TRUE(bounds(9));
    EXPECT_TRUE(bounds(5));
    EXPECT_FALSE(bounds(-1));
    EXPECT_FALSE(bounds(11));
}

TEST(NumericBoundsTest, IntegerExclusiveAdjacentOnlyValueAccepted)
{
    // exclusive(5, 7) accepts only 6 — the single integer strictly between 5 and 7.
    auto bounds = vd::int_bounds::exclusive(5, 7);
    EXPECT_FALSE(bounds(5));
    EXPECT_TRUE(bounds(6));
    EXPECT_FALSE(bounds(7));
}

TEST(NumericBoundsTest, IntegerGreaterThanStepsByOne)
{
    auto bounds = vd::int_bounds::greater_than(5);
    EXPECT_FALSE(bounds(5));
    EXPECT_FALSE(bounds(4));
    EXPECT_TRUE(bounds(6));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::max()));
}

TEST(NumericBoundsTest, IntegerLessThanStepsByOne)
{
    auto bounds = vd::int_bounds::less_than(5);
    EXPECT_FALSE(bounds(5));
    EXPECT_FALSE(bounds(6));
    EXPECT_TRUE(bounds(4));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::min()));
}

// ---------------------------------------------------------------------------
// numeric_bounds — float/double exclusive immediate neighbor
// The bound is set to the representable value adjacent to the endpoint, so the
// immediate neighbor must be accepted and the endpoint itself rejected.
// ---------------------------------------------------------------------------

TEST(NumericBoundsTest, DoubleExclusiveAcceptsImmediateNeighbor)
{
    auto bounds = vd::double_bounds::exclusive(0.0, 1.0);
    EXPECT_TRUE(bounds(std::nextafter(0.0, 1.0)));
    EXPECT_TRUE(bounds(std::nextafter(1.0, 0.0)));
}

TEST(NumericBoundsTest, FloatExclusiveAcceptsImmediateNeighbor)
{
    auto bounds = vd::float_bounds::exclusive(0.0f, 1.0f);
    EXPECT_FALSE(bounds(0.0f));
    EXPECT_FALSE(bounds(1.0f));
    EXPECT_TRUE(bounds(std::nextafter(0.0f, 1.0f)));
    EXPECT_TRUE(bounds(std::nextafter(1.0f, 0.0f)));
}

/// !!! IMPLEMENT ME !!!
/// Constexpr tests for numeric_bounds - temporary disabled due to transition into vd::result-based API.
/// This would be great to make vd::result-based checkers usable in constant expressions as well, but that requires vd::result to be
/// constexpr-friendly.
// ---------------------------------------------------------------------------

// // ---------------------------------------------------------------------------
// // numeric_bounds — constexpr evaluation
// // All six factory methods and operator() must be usable in constant expressions.
// // ---------------------------------------------------------------------------

// TEST(NumericBoundsConstexprTest, InclusiveEvaluatesAtCompileTime)
// {
//     constexpr auto bounds = vd::int_bounds::inclusive(0, 10);
//     static_assert(bounds(0));
//     static_assert(bounds(10));
//     static_assert(bounds(5));
//     static_assert(!bounds(-1));
//     static_assert(!bounds(11));
//     SUCCEED();
// }

// TEST(NumericBoundsConstexprTest, ExclusiveEvaluatesAtCompileTime)
// {
//     constexpr auto bounds = vd::int_bounds::exclusive(0, 10);
//     static_assert(!bounds(0));
//     static_assert(!bounds(10));
//     static_assert(bounds(1));
//     static_assert(bounds(9));
//     static_assert(bounds(5));
//     SUCCEED();
// }

// TEST(NumericBoundsConstexprTest, GreaterThanEvaluatesAtCompileTime)
// {
//     constexpr auto bounds = vd::int_bounds::greater_than(5);
//     static_assert(!bounds(5));
//     static_assert(bounds(6));
//     SUCCEED();
// }

// TEST(NumericBoundsConstexprTest, LessThanEvaluatesAtCompileTime)
// {
//     constexpr auto bounds = vd::int_bounds::less_than(5);
//     static_assert(!bounds(5));
//     static_assert(bounds(4));
//     SUCCEED();
// }

// TEST(NumericBoundsConstexprTest, UnboundedEvaluatesAtCompileTime)
// {
//     constexpr auto bounds = vd::double_bounds::unbounded();
//     static_assert(bounds(0.0));
//     SUCCEED();
// }

// ---------------------------------------------------------------------------
// vd::rule — direct construction from callable
// Verifies that rule<T> correctly wraps and invokes an arbitrary predicate.
// ---------------------------------------------------------------------------

TEST(RuleTest, PredicateCalledWithCorrectObject)
{
    Point captured {};
    vd::rule<Point> r([&captured](const Point& p) {
        captured = p;
        return true;
    });

    Point p { 3.0, 4.0 };
    r(p);

    EXPECT_EQ(captured.x, 3.0);
    EXPECT_EQ(captured.y, 4.0);
}

TEST(RuleTest, PointerOverloadDereferencesCorrectly)
{
    // operator()(const T*) must dereference and call the predicate — not skip it.
    vd::rule<Point> r([](const Point& p) {
        return p.x > 0;
    });

    Point p { 1.0, 0.0 };
    EXPECT_TRUE(r(&p));

    Point q { -1.0, 0.0 };
    EXPECT_FALSE(r(&q));
}

TEST(RuleTest, ReturnsPredicateResult)
{
    vd::rule<int> always_true([](const int&) {
        return true;
    });
    vd::rule<int> always_false([](const int&) {
        return false;
    });

    int v = 42;
    EXPECT_TRUE(always_true(v));
    EXPECT_FALSE(always_false(v));
}

// ---------------------------------------------------------------------------
// vd::predicate — factory, T deduced from lambda signature
// ---------------------------------------------------------------------------

TEST(PredicateFactoryTest, DeducesTypeFromLambda)
{
    // T should be deduced as Point — no explicit template argument needed.
    auto rule = vd::predicate([](const Point& p) {
        return p.x >= 0 && p.y >= 0;
    });

    EXPECT_TRUE(rule(Point { 1.0, 1.0 }));
    EXPECT_FALSE(rule(Point { -1.0, 1.0 }));
}

TEST(PredicateFactoryTest, WorksWithMultipleFields)
{
    auto rule = vd::predicate([](const NamedValue& nv) {
        return !nv.name.empty() && nv.value > 0;
    });

    EXPECT_TRUE(rule(NamedValue { "ok", 1 }));
    EXPECT_FALSE(rule(NamedValue { "", 1 }));
    EXPECT_FALSE(rule(NamedValue { "ok", 0 }));
}

// ---------------------------------------------------------------------------
// vd::field — member function pointer + checker
// ---------------------------------------------------------------------------

TEST(FieldFactoryTest, InvokesGetterAndChecksResult)
{
    auto rule = vd::field(&Point::get_x, vd::double_bounds::inclusive(0.0, 10.0));

    EXPECT_TRUE(rule(Point { 5.0, 0.0 }));
    EXPECT_FALSE(rule(Point { -1.0, 0.0 }));
    EXPECT_FALSE(rule(Point { 11.0, 0.0 }));
}

TEST(FieldFactoryTest, BoundaryValuesAreAccepted)
{
    auto rule = vd::field(&Point::get_x, vd::double_bounds::inclusive(0.0, 10.0));

    EXPECT_TRUE(rule(Point { 0.0, 0.0 }));
    EXPECT_TRUE(rule(Point { 10.0, 0.0 }));
}

TEST(FieldFactoryTest, WorksWithNonTrivialGetter)
{
    // magnitude() = x*x + y*y — verifies that any const member function works,
    // not only simple accessors. less_than is exclusive, so 25.0 itself is rejected.
    auto rule = vd::field(&Point::magnitude, vd::double_bounds::less_than(25.0));

    EXPECT_TRUE(rule(Point { 3.0, 0.0 }));  // magnitude = 9.0  < 25.0 → passes
    EXPECT_FALSE(rule(Point { 3.0, 4.0 })); // magnitude = 25.0, less_than is exclusive → rejects
    EXPECT_FALSE(rule(Point { 5.0, 5.0 })); // magnitude = 50.0 > 25.0 → rejects
}

TEST(FieldFactoryTest, WorksWithStringGetter)
{
    // Checks that vd::field works with non-numeric types returned from the getter,
    // as long as the checker accepts that type.
    auto rule = vd::field(&NamedValue::get_name, [](const std::string& s) {
        return !s.empty();
    });

    EXPECT_TRUE(rule(NamedValue { "hello", 0 }));
    EXPECT_FALSE(rule(NamedValue { "", 0 }));
}

// ---------------------------------------------------------------------------
// vd::member — member variable pointer + checker
// ---------------------------------------------------------------------------

TEST(MemberFactoryTest, AccessesFieldDirectly)
{
    auto rule = vd::member(&Point::x, vd::double_bounds::greater_than(0.0));

    EXPECT_TRUE(rule(Point { 0.1, 0.0 }));
    EXPECT_FALSE(rule(Point { 0.0, 0.0 }));
    EXPECT_FALSE(rule(Point { -1.0, 0.0 }));
}

TEST(MemberFactoryTest, WorksWithIntField)
{
    auto rule = vd::member(&NamedValue::value, vd::int_bounds::inclusive(1, 100));

    EXPECT_TRUE(rule(NamedValue { "x", 50 }));
    EXPECT_FALSE(rule(NamedValue { "x", 0 }));
    EXPECT_FALSE(rule(NamedValue { "x", 101 }));
}

// ---------------------------------------------------------------------------
// vd::basic_model — composition of rules via with()
// ---------------------------------------------------------------------------

TEST(BasicModelTest, EmptyModelAcceptsAnything)
{
    // A model with no rules should treat every object as valid.
    vd::basic_model<Point> model;
    EXPECT_TRUE(model.is_valid(Point { -999.0, -999.0 }));
}

TEST(BasicModelTest, SingleRuleAcceptedObject)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.is_valid(Point { 5.0, 0.0 }));
}

TEST(BasicModelTest, SingleRuleRejectedObject)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_FALSE(model.is_valid(Point { -1.0, 0.0 }));
}

TEST(BasicModelTest, AllRulesMustPassForValid)
{
    // Both x and y must be in [0, 10]. Object fails if either rule fails.
    auto model = vd::basic_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(model.is_valid(Point { 5.0, 5.0 }));
    EXPECT_FALSE(model.is_valid(Point { -1.0, 5.0 }));  // x fails
    EXPECT_FALSE(model.is_valid(Point { 5.0, -1.0 }));  // y fails
    EXPECT_FALSE(model.is_valid(Point { -1.0, -1.0 })); // both fail
}

TEST(BasicModelTest, InitializerListConstructor)
{
    vd::basic_model<Point> model({
        vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)),
        vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)),
    });

    EXPECT_TRUE(model.is_valid(Point { 1.0, 1.0 }));
    EXPECT_FALSE(model.is_valid(Point { 1.0, 20.0 }));
}

TEST(BasicModelTest, WithInitializerListAddsAllRules)
{
    auto model = vd::basic_model<Point>().with({
        vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)),
        vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)),
    });

    EXPECT_TRUE(model.is_valid(Point { 5.0, 5.0 }));
    EXPECT_FALSE(model.is_valid(Point { 5.0, 11.0 }));
}

TEST(BasicModelTest, WithMergesAnotherModel)
{
    // Two models merged via with(model) must enforce the union of both rule sets.
    auto x_model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    auto y_model = vd::basic_model<Point>().with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    auto combined = vd::basic_model<Point>().with(x_model).with(y_model);

    EXPECT_TRUE(combined.is_valid(Point { 5.0, 5.0 }));
    EXPECT_FALSE(combined.is_valid(Point { -1.0, 5.0 }));
    EXPECT_FALSE(combined.is_valid(Point { 5.0, -1.0 }));
}

TEST(BasicModelTest, AddRuleAppendsDynamically)
{
    vd::basic_model<Point> model;
    model.add_rule(vd::member(&Point::x, vd::double_bounds::greater_than(0.0)));

    EXPECT_TRUE(model.is_valid(Point { 1.0, 0.0 }));
    EXPECT_FALSE(model.is_valid(Point { 0.0, 0.0 }));
}

TEST(BasicModelTest, IsValidByPointer)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { 5.0, 0.0 };
    EXPECT_TRUE(model.is_valid(&p));

    Point q { -1.0, 0.0 };
    EXPECT_FALSE(model.is_valid(&q));
}

// ---------------------------------------------------------------------------
// vd::basic_bound_model — bound model carries the object reference
// ---------------------------------------------------------------------------

TEST(BoundModelTest, IsValidDelegatesToModel)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { 5.0, 0.0 };
    auto bound = model.bind(p);
    EXPECT_TRUE(bound.is_valid());

    Point q { -1.0, 0.0 };
    auto bound_invalid = model.bind(q);
    EXPECT_FALSE(bound_invalid.is_valid());
}

TEST(BoundModelTest, BindByPointerAbortOnNull)
{
    // Passing a null pointer to bind() must abort — it violates the precondition.
    auto model = vd::basic_model<Point>();
    EXPECT_DEATH(model.bind(static_cast<const Point*>(nullptr)), "");
}

// ---------------------------------------------------------------------------
// vd::basic_model::dead_check — throws validation_exception on invalid object
// ---------------------------------------------------------------------------

TEST(DeadCheckModelTest, ValidObjectDoesNotThrow)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_NO_THROW(model.die_if_failed(Point { 5.0, 0.0 }));
}

TEST(DeadCheckModelTest, InvalidObjectThrowsValidationException)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_THROW(model.die_if_failed(Point { -1.0, 0.0 }), vd::validation_exception);
}

TEST(DeadCheckModelTest, ThrownExceptionIsCaughtAsStdException)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_THROW(model.die_if_failed(Point { -1.0, 0.0 }), std::exception);
}

TEST(DeadCheckModelTest, ValidPointerDoesNotThrow)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { 5.0, 0.0 };
    EXPECT_NO_THROW(model.die_if_failed(&p));
}

TEST(DeadCheckModelTest, InvalidPointerThrowsValidationException)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { -1.0, 0.0 };
    EXPECT_THROW(model.die_if_failed(&p), vd::validation_exception);
}

TEST(DeadCheckModelTest, NullPointerThrowsValidationException)
{
    // nullptr is treated as an invalid object by is_valid(), so die_if_failed must throw.
    auto model = vd::basic_model<Point>();
    EXPECT_THROW(model.die_if_failed(static_cast<const Point*>(nullptr)), vd::validation_exception);
}

TEST(DeadCheckModelTest, MultipleRulesThrowOnFirstViolation)
{
    auto model = vd::basic_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_NO_THROW(model.die_if_failed(Point { 5.0, 5.0 }));
    EXPECT_THROW(model.die_if_failed(Point { -1.0, 5.0 }), vd::validation_exception);
    EXPECT_THROW(model.die_if_failed(Point { 5.0, -1.0 }), vd::validation_exception);
}

// ---------------------------------------------------------------------------
// vd::basic_bound_model::dead_check — delegates to the underlying model
// ---------------------------------------------------------------------------

TEST(DeadCheckBoundModelTest, ValidObjectDoesNotThrow)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { 5.0, 0.0 };
    EXPECT_NO_THROW(model.bind(p).die_if_failed());
}

TEST(DeadCheckBoundModelTest, InvalidObjectThrowsValidationException)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { -1.0, 0.0 };
    EXPECT_THROW(model.bind(p).die_if_failed(), vd::validation_exception);
}

TEST(DeadCheckBoundModelTest, ThrownExceptionIsCaughtAsStdException)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point p { -1.0, 0.0 };
    EXPECT_THROW(model.bind(p).die_if_failed(), std::exception);
}

// ---------------------------------------------------------------------------
// Mixed: field + member + predicate in a single model
// ---------------------------------------------------------------------------

TEST(MixedRulesTest, AllFactoryTypesCompose)
{
    // x must be in [0, 100] via member access,
    // magnitude must be < 10000 via getter,
    // and the predicate checks that neither coordinate is NaN.
    auto model = vd::basic_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
                     .with(vd::field(&Point::magnitude, vd::double_bounds::less_than(10000.0)))
                     .with(vd::predicate([](const Point& p) {
                         return !std::isnan(p.x) && !std::isnan(p.y);
                     }));

    EXPECT_TRUE(model.is_valid(Point { 3.0, 4.0 }));
    EXPECT_FALSE(model.is_valid(Point { -1.0, 4.0 }));  // x out of range
    EXPECT_FALSE(model.is_valid(Point { 99.0, 99.0 })); // magnitude too large
    EXPECT_FALSE(model.is_valid(Point { std::numeric_limits<double>::quiet_NaN(), 0.0 }));
}

// ---------------------------------------------------------------------------
// numeric_bounds — outside_inclusive
// Accepts values in (-inf, lower_bound] ∪ [upper_bound, +inf).
// Rejects values strictly inside (lower_bound, upper_bound).
// ---------------------------------------------------------------------------

TEST(NumericBoundsOutsideInclusiveTest, AcceptsLowerBoundItself)
{
    auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_TRUE(bounds(0));
}

TEST(NumericBoundsOutsideInclusiveTest, AcceptsUpperBoundItself)
{
    auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_TRUE(bounds(10));
}

TEST(NumericBoundsOutsideInclusiveTest, AcceptsBelowLowerBound)
{
    auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_TRUE(bounds(-1));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::min()));
}

TEST(NumericBoundsOutsideInclusiveTest, AcceptsAboveUpperBound)
{
    auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_TRUE(bounds(11));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::max()));
}

TEST(NumericBoundsOutsideInclusiveTest, RejectsStrictlyInsideRange)
{
    auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_FALSE(bounds(1));
    EXPECT_FALSE(bounds(5));
    EXPECT_FALSE(bounds(9));
}

TEST(NumericBoundsOutsideInclusiveTest, DoubleAcceptsExactBoundaries)
{
    auto bounds = vd::double_bounds::outside_inclusive(0.0, 1.0);
    EXPECT_TRUE(bounds(0.0));
    EXPECT_TRUE(bounds(1.0));
    EXPECT_FALSE(bounds(0.5));
}

TEST(NumericBoundsOutsideInclusiveTest, DoubleAcceptsOutsideRegion)
{
    auto bounds = vd::double_bounds::outside_inclusive(0.0, 1.0);
    EXPECT_TRUE(bounds(-0.1));
    EXPECT_TRUE(bounds(1.1));
}

TEST(NumericBoundsOutsideInclusiveTest, ConstructibleAsConstexpr)
{
    // Verifies that outside_inclusive is usable in a constant expression context.
    constexpr auto bounds = vd::int_bounds::outside_inclusive(0, 10);
    EXPECT_TRUE(bounds(0));
    EXPECT_TRUE(bounds(10));
    EXPECT_TRUE(bounds(-1));
    EXPECT_TRUE(bounds(11));
    EXPECT_FALSE(bounds(5));
}

// ---------------------------------------------------------------------------
// numeric_bounds — outside_exclusive
// Accepts values in (-inf, lower_bound) ∪ (upper_bound, +inf).
// Rejects values in [lower_bound, upper_bound].
// ---------------------------------------------------------------------------

TEST(NumericBoundsOutsideExclusiveTest, RejectsLowerBoundItself)
{
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_FALSE(bounds(0));
}

TEST(NumericBoundsOutsideExclusiveTest, RejectsUpperBoundItself)
{
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_FALSE(bounds(10));
}

TEST(NumericBoundsOutsideExclusiveTest, RejectsStrictlyInsideRange)
{
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_FALSE(bounds(1));
    EXPECT_FALSE(bounds(5));
    EXPECT_FALSE(bounds(9));
}

TEST(NumericBoundsOutsideExclusiveTest, AcceptsStrictlyBelowLowerBound)
{
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_TRUE(bounds(-1));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::min()));
}

TEST(NumericBoundsOutsideExclusiveTest, AcceptsStrictlyAboveUpperBound)
{
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_TRUE(bounds(11));
    EXPECT_TRUE(bounds(std::numeric_limits<int32_t>::max()));
}

TEST(NumericBoundsOutsideExclusiveTest, IntegerAdjacentToBoundariesAreAccepted)
{
    // For integers, nextafter steps by ±1, so -1 and 11 are the immediate neighbors.
    auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_TRUE(bounds(-1));
    EXPECT_TRUE(bounds(11));
}

TEST(NumericBoundsOutsideExclusiveTest, DoubleRejectsExactBoundaries)
{
    auto bounds = vd::double_bounds::outside_exclusive(0.0, 1.0);
    EXPECT_FALSE(bounds(0.0));
    EXPECT_FALSE(bounds(1.0));
    EXPECT_FALSE(bounds(0.5));
}

TEST(NumericBoundsOutsideExclusiveTest, DoubleAcceptsImmediateNeighbors)
{
    auto bounds = vd::double_bounds::outside_exclusive(0.0, 1.0);
    EXPECT_TRUE(bounds(std::nextafter(0.0, -1.0)));
    EXPECT_TRUE(bounds(std::nextafter(1.0, 2.0)));
}

TEST(NumericBoundsOutsideExclusiveTest, ConstructibleAsConstexpr)
{
    // Verifies that outside_exclusive is usable in a constant expression context.
    constexpr auto bounds = vd::int_bounds::outside_exclusive(0, 10);
    EXPECT_FALSE(bounds(0));
    EXPECT_FALSE(bounds(10));
    EXPECT_FALSE(bounds(5));
    EXPECT_TRUE(bounds(-1));
    EXPECT_TRUE(bounds(11));
}

// ---------------------------------------------------------------------------
// vd::validate_many — variadic overload (pack of T by value/ref)
// ---------------------------------------------------------------------------

TEST(ValidateManyVariadicTest, AllValidObjectsReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { 5.0, 0.0 }, Point { 10.0, 0.0 }));
}

TEST(ValidateManyVariadicTest, OneInvalidObjectReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_FALSE(vd::validate_many(model, Point { 1.0, 0.0 }, Point { -1.0, 0.0 }, Point { 5.0, 0.0 }));
}

TEST(ValidateManyVariadicTest, AllInvalidObjectsReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_FALSE(vd::validate_many(model, Point { -1.0, 0.0 }, Point { 11.0, 0.0 }));
}

TEST(ValidateManyVariadicTest, SingleValidObjectReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(vd::validate_many(model, Point { 5.0, 0.0 }));
}

TEST(ValidateManyVariadicTest, SingleInvalidObjectReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_FALSE(vd::validate_many(model, Point { -5.0, 0.0 }));
}

TEST(ValidateManyVariadicTest, MultipleRulesAllMustPassForEveryObject)
{
    auto model = vd::basic_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(vd::validate_many(model, Point { 1.0, 1.0 }, Point { 5.0, 5.0 }));
    EXPECT_FALSE(vd::validate_many(model, Point { 1.0, 1.0 }, Point { 5.0, 11.0 })); // second y fails
}

// ---------------------------------------------------------------------------
// vd::validate_many — vector<T> overload
// ---------------------------------------------------------------------------

TEST(ValidateManyVectorTest, EmptyVectorReturnsTrue)
{
    // all_of on an empty range is vacuously true.
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    EXPECT_TRUE(vd::validate_many(model, std::vector<Point> {}));
}

TEST(ValidateManyVectorTest, AllValidElementsReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<Point> pts { { 0.0, 0.0 }, { 5.0, 0.0 }, { 10.0, 0.0 } };
    EXPECT_TRUE(vd::validate_many(model, pts));
}

TEST(ValidateManyVectorTest, OneInvalidElementReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<Point> pts { { 1.0, 0.0 }, { -1.0, 0.0 }, { 5.0, 0.0 } };
    EXPECT_FALSE(vd::validate_many(model, pts));
}

TEST(ValidateManyVectorTest, AllInvalidElementsReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<Point> pts { { -1.0, 0.0 }, { 11.0, 0.0 } };
    EXPECT_FALSE(vd::validate_many(model, pts));
}

TEST(ValidateManyVectorTest, MultipleRulesAppliedToEachElement)
{
    auto model = vd::basic_model<Point>()
                     .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                     .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<Point> all_valid { { 1.0, 1.0 }, { 5.0, 5.0 }, { 10.0, 10.0 } };
    EXPECT_TRUE(vd::validate_many(model, all_valid));

    std::vector<Point> one_bad { { 1.0, 1.0 }, { 5.0, 15.0 } }; // second y out of range
    EXPECT_FALSE(vd::validate_many(model, one_bad));
}

// ---------------------------------------------------------------------------
// vd::validate_many — vector<T*> overload (non-owning mutable pointers)
// ---------------------------------------------------------------------------

TEST(ValidateManyPtrVectorTest, AllValidPointersReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point a { 1.0, 0.0 }, b { 5.0, 0.0 }, c { 10.0, 0.0 };
    std::vector<Point*> ptrs { &a, &b, &c };
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyPtrVectorTest, OneInvalidPointerReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    Point a { 1.0, 0.0 }, b { -1.0, 0.0 }, c { 5.0, 0.0 };
    std::vector<Point*> ptrs { &a, &b, &c };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyPtrVectorTest, EmptyVectorReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<Point*> ptrs {};
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyPtrVectorTest, NullPointerInVectorReturnsFalse)
{
    // is_valid(nullptr) returns false, so a null element must fail the whole check.
    auto model = vd::basic_model<Point>();

    Point a { 1.0, 0.0 };
    std::vector<Point*> ptrs { &a, nullptr };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

// ---------------------------------------------------------------------------
// vd::validate_many — vector<const T*> overload (non-owning const pointers)
// ---------------------------------------------------------------------------

TEST(ValidateManyConstPtrVectorTest, AllValidConstPointersReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    const Point a { 2.0, 0.0 }, b { 8.0, 0.0 };
    std::vector<const Point*> ptrs { &a, &b };
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyConstPtrVectorTest, OneInvalidConstPointerReturnsFalse)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    const Point a { 2.0, 0.0 }, b { 15.0, 0.0 };
    std::vector<const Point*> ptrs { &a, &b };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyConstPtrVectorTest, EmptyVectorReturnsTrue)
{
    auto model = vd::basic_model<Point>().with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

    std::vector<const Point*> ptrs {};
    EXPECT_TRUE(vd::validate_many(model, ptrs));
}

TEST(ValidateManyConstPtrVectorTest, NullPointerInVectorReturnsFalse)
{
    auto model = vd::basic_model<Point>();

    const Point a { 1.0, 0.0 };
    std::vector<const Point*> ptrs { &a, nullptr };
    EXPECT_FALSE(vd::validate_many(model, ptrs));
}
