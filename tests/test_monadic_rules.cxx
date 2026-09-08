#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <version>

#if defined(__cpp_lib_expected)
#include <expected>
#endif

#include <vd.hxx>

// ---------------------------------------------------------------------------
// Test fixtures
//
// vd::monadic rules operate on the monadic wrapper itself (std::optional /
// std::expected), so the fixtures deliberately expose those wrappers both as
// plain members and through getters — the two checker entry points.
// ---------------------------------------------------------------------------

struct UserSettings {
    std::optional<std::string> nickname;
    std::optional<std::int32_t> port;

    const std::optional<std::string>& get_nickname() const
    {
        return nickname;
    }
};

struct Telemetry {
    std::optional<double> sample;
    double threshold = 0.0;
};

// ---------------------------------------------------------------------------
// MonadicNotEmptyTest — the rule in isolation
//
// not_empty is a single shared `inline constexpr` object with a templated
// operator(); the payload type is deduced from the argument, so the same object
// serves every std::optional<T>.
//
// It checks *engagement*, never the truthiness of the contained value. An
// engaged optional holding 0, false or "" must therefore still pass.
// ---------------------------------------------------------------------------

TEST(MonadicNotEmptyTest, EngagedOptionalPasses)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::int32_t> { 42 }));
}

TEST(MonadicNotEmptyTest, DisengagedOptionalFails)
{
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<std::int32_t> {}));
}

TEST(MonadicNotEmptyTest, FailureCarriesDescription)
{
    vd::result res = vd::monadic::not_empty(std::optional<std::int32_t> {});

    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("must not be empty"), std::string::npos);
}

TEST(MonadicNotEmptyTest, SuccessCarriesNoDescription)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::int32_t> { 0 }).failed_rules.empty());
}

// Engagement, not truthiness: a zero / false / empty-string payload is present.
TEST(MonadicNotEmptyTest, EngagedWithZeroPasses)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::int32_t> { 0 }));
}

TEST(MonadicNotEmptyTest, EngagedWithFalsePasses)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<bool> { false }));
}

TEST(MonadicNotEmptyTest, EngagedWithEmptyStringPasses)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::string> { std::string {} }));
}

TEST(MonadicNotEmptyTest, WorksWithNonTrivialValueType)
{
    using vec_t = std::vector<std::int32_t>;

    EXPECT_TRUE(vd::monadic::not_empty(std::optional<vec_t> { vec_t { 1, 2, 3 } }));
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<vec_t> {}));
}

// The point of the templated operator(): one object, every payload type, no
// template argument at the call site.
TEST(MonadicNotEmptyTest, OneObjectServesEveryPayloadType)
{
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::int32_t> { 1 }));
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::string> { std::string { "a" } }));
    EXPECT_TRUE(vd::monadic::not_empty(std::optional<double> { 0.5 }));
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<std::int32_t> {}));
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<std::string> {}));
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<double> {}));
}

// The rule is stateless, so the shared object is reusable and freely copyable.
TEST(MonadicNotEmptyTest, RuleInstanceIsReusable)
{
    auto copy = vd::monadic::not_empty;

    EXPECT_TRUE(vd::monadic::not_empty(std::optional<std::int32_t> { 1 }));
    EXPECT_FALSE(vd::monadic::not_empty(std::optional<std::int32_t> {}));
    EXPECT_TRUE(copy(std::optional<std::int32_t> { 1 }));
    EXPECT_FALSE(copy(std::optional<std::int32_t> {}));
}

// ---------------------------------------------------------------------------
// MonadicRuleConceptTest — type-level contract
//
// The rules must satisfy both rule concepts so they can be used either as a
// value checker (vd::member / vd::field) or as a top-level static_model rule.
// ---------------------------------------------------------------------------

TEST(MonadicRuleConceptTest, NotEmptySatisfiesValueChecker)
{
    using rule_t = vd::monadic::not_empty_t;
    static_assert(vd::value_checker<rule_t, std::optional<std::int32_t>>);
    static_assert(vd::value_checker<rule_t, const std::optional<std::int32_t>&>);
    SUCCEED();
}

TEST(MonadicRuleConceptTest, NotEmptySatisfiesStaticRuleFor)
{
    static_assert(vd::static_rule_for<vd::monadic::not_empty_t, std::optional<std::int32_t>>);
    SUCCEED();
}

TEST(MonadicRuleConceptTest, NotEmptyReturnsResultNotBool)
{
    using rule_t = vd::monadic::not_empty_t;
    static_assert(std::is_same_v<std::invoke_result_t<rule_t, const std::optional<std::int32_t>&>, vd::result>);
    SUCCEED();
}

// The shared object is `inline constexpr`, i.e. const — operator() has to be
// const-qualified for it to be callable at all, both directly and from inside
// the const lambdas the rule factories build.
TEST(MonadicRuleConceptTest, SharedObjectIsConstInvocable)
{
    static_assert(std::is_const_v<std::remove_reference_t<decltype(vd::monadic::not_empty)>>);
    static_assert(vd::value_checker<const vd::monadic::not_empty_t&, std::optional<std::int32_t>>);
    static_assert(vd::static_rule_for<const vd::monadic::not_empty_t&, std::optional<std::int32_t>>);
    SUCCEED();
}

// Deduction is the type filter: anything that is not a std::optional fails
// template argument deduction in the immediate context, so the concepts report
// a clean `false` instead of hard-erroring inside an instantiation.
TEST(MonadicRuleConceptTest, NotEmptyRejectsNonOptionalTypes)
{
    static_assert(!vd::value_checker<vd::monadic::not_empty_t, std::int32_t>);
    static_assert(!vd::value_checker<vd::monadic::not_empty_t, std::string>);
    SUCCEED();
}

// A bare std::nullopt is *not* accepted any more: T cannot be deduced from
// std::nullopt_t. Spell the optional out — std::optional<T> {} — instead.
TEST(MonadicRuleConceptTest, NotEmptyRejectsBareNullopt)
{
    static_assert(!vd::value_checker<vd::monadic::not_empty_t, std::nullopt_t>);
    SUCCEED();
}

// The rule is an empty, constexpr-constructible type, so a copy can be made in
// a constant expression even though evaluating it cannot be (vd::result owns a
// vector).
TEST(MonadicRuleConceptTest, NotEmptyIsEmptyAndConstexprConstructible)
{
    static_assert(std::is_empty_v<vd::monadic::not_empty_t>);

    constexpr auto rule = vd::monadic::not_empty;
    EXPECT_TRUE(rule(std::optional<std::int32_t> { 1 }));
}

// ---------------------------------------------------------------------------
// MonadicNotEmptyBasicModelTest — integration with basic_model<T>
//
// Two distinct entry points are covered:
//   * as a *checker* handed to vd::member / vd::field, where the model's T is
//     the owning struct — the common case, needs no wrapping;
//   * as a *top-level rule* over an optional-typed model, which must be wrapped
//     in vd::rule<T> explicitly. vd::predicate cannot be used here: it deduces
//     T via detail::first_arg_of, which takes the address of operator() —
//     impossible for a templated one. See docs/extending.md.
// ---------------------------------------------------------------------------

TEST(MonadicNotEmptyBasicModelTest, MemberCheckerAcceptsEngagedOptional)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));
    EXPECT_TRUE(model.check(UserSettings { std::string { "ada" }, 8080 }));
}

TEST(MonadicNotEmptyBasicModelTest, MemberCheckerRejectsDisengagedOptional)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));
    EXPECT_FALSE(model.check(UserSettings { std::nullopt, 8080 }));
}

TEST(MonadicNotEmptyBasicModelTest, MemberCheckerFailurePropagatesMemberName)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty));
    vd::result res = model.check(UserSettings { std::nullopt, 8080 });

    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("nickname"), std::string::npos);
    EXPECT_NE(res.failed_rules[0].find("must not be empty"), std::string::npos);
}

TEST(MonadicNotEmptyBasicModelTest, FieldCheckerViaGetter)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::field("nickname", &UserSettings::get_nickname, vd::monadic::not_empty));

    EXPECT_TRUE(model.check(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_FALSE(model.check(UserSettings { std::nullopt, 8080 }));
}

// One shared object, two members of different payload type in the same model —
// the old API needed a separate not_empty<T>() instantiation spelled out per
// member.
TEST(MonadicNotEmptyBasicModelTest, SameObjectChecksMembersOfDifferentPayloadTypes)
{
    auto model = vd::basic_model<UserSettings> {}
                     .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                     .with(vd::member("port", &UserSettings::port, vd::monadic::not_empty));

    EXPECT_TRUE(model.check(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_FALSE(model.check(UserSettings { std::string { "ada" }, std::nullopt }));
    EXPECT_FALSE(model.check(UserSettings { std::nullopt, 8080 }));
}

TEST(MonadicNotEmptyBasicModelTest, TopLevelRuleViaExplicitRuleWrap)
{
    // basic_model stores vd::rule<T>; rule's converting constructor is explicit,
    // so a bare rule object has to be wrapped before it can be added. The wrap
    // is also what pins down T, which the templated operator() cannot supply.
    auto model = vd::basic_model<std::optional<std::int32_t>> {}.with(vd::rule<std::optional<std::int32_t>>(vd::monadic::not_empty));

    EXPECT_TRUE(model.check(std::optional<std::int32_t> { 7 }));
    EXPECT_FALSE(model.check(std::optional<std::int32_t> {}));
}

TEST(MonadicNotEmptyBasicModelTest, InitializerListConstructionWithWrappedRule)
{
    vd::basic_model<std::optional<std::string>> model { vd::rule<std::optional<std::string>>(vd::monadic::not_empty) };

    EXPECT_TRUE(model.check(std::optional<std::string> { std::string { "x" } }));
    EXPECT_FALSE(model.check(std::optional<std::string> {}));
}

TEST(MonadicNotEmptyBasicModelTest, CheckAggregatesEveryEmptyOptional)
{
    auto model = vd::basic_model<UserSettings> {}
                     .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                     .with(vd::member("port", &UserSettings::port, vd::monadic::not_empty));

    vd::result res = model.check(UserSettings { std::nullopt, std::nullopt });

    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.failed_rules.size(), 2u);
}

TEST(MonadicNotEmptyBasicModelTest, ShortCheckStopsAtFirstEmptyOptional)
{
    auto model = vd::basic_model<UserSettings> {}
                     .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                     .with(vd::member("port", &UserSettings::port, vd::monadic::not_empty));

    const UserSettings bad { std::nullopt, std::nullopt };

    // both overloads must short-circuit alike — the reference one delegates to
    // the pointer one, so a regression there would silently turn it into check()
    for(vd::result res : { model.short_check(bad), model.short_check(&bad) }) {
        EXPECT_FALSE(res.is_valid);
        EXPECT_EQ(res.failed_rules.size(), 1u);
        EXPECT_NE(res.failed_rules[0].find("nickname"), std::string::npos);
    }
}

TEST(MonadicNotEmptyBasicModelTest, DieIfFailedThrowsOnEmptyOptional)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));

    EXPECT_NO_THROW(model.die_if_failed(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_THROW(model.die_if_failed(UserSettings { std::nullopt, 8080 }), vd::validation_exception);
}

TEST(MonadicNotEmptyBasicModelTest, BoundModelChecksEngagement)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));

    const UserSettings good { std::string { "ada" }, 8080 };
    const UserSettings bad { std::nullopt, 8080 };

    EXPECT_TRUE(model.bind(good).check());
    EXPECT_FALSE(model.bind(bad).check());
}

TEST(MonadicNotEmptyBasicModelTest, ValidateManyRejectsBatchWithEmptyOptional)
{
    auto model = vd::basic_model<UserSettings> {}.with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));

    EXPECT_TRUE(vd::validate_many(model, UserSettings { std::string { "a" }, 1 }, UserSettings { std::string { "b" }, 2 }));
    EXPECT_FALSE(vd::validate_many(model, UserSettings { std::string { "a" }, 1 }, UserSettings { std::nullopt, 2 }));
}

// ---------------------------------------------------------------------------
// MonadicNotEmptyStaticModelTest — integration with static_model<T, Rules...>
//
// static_model stores rules by value in a tuple, so the bare rule object is
// accepted directly — no vd::rule wrap and no heap allocation. static_rule_for
// already knows T, so nothing has to be deduced from the checker.
// ---------------------------------------------------------------------------

TEST(MonadicNotEmptyStaticModelTest, TopLevelRuleOverOptional)
{
    auto model = vd::make_static_model<std::optional<std::int32_t>>().with(vd::monadic::not_empty);

    EXPECT_TRUE(model.check(std::optional<std::int32_t> { 7 }));
    EXPECT_FALSE(model.check(std::optional<std::int32_t> {}));
}

TEST(MonadicNotEmptyStaticModelTest, TopLevelRuleRejectsNullptr)
{
    auto model = vd::make_static_model<std::optional<std::int32_t>>().with(vd::monadic::not_empty);
    EXPECT_FALSE(model.check(static_cast<const std::optional<std::int32_t>*>(nullptr)));
}

TEST(MonadicNotEmptyStaticModelTest, ModelIsConstexprConstructible)
{
    // The rule is stateless and constexpr-constructible, so the whole model can
    // be built at compile time; only check() has to run at runtime.
    constexpr auto model = vd::make_static_model<std::optional<std::int32_t>>().with(vd::monadic::not_empty);

    EXPECT_TRUE(model.check(std::optional<std::int32_t> { 7 }));
    EXPECT_FALSE(model.check(std::optional<std::int32_t> {}));
}

TEST(MonadicNotEmptyStaticModelTest, ModelStoresNoIndirection)
{
    // A stateless rule must not inflate the model — evidence that static_model
    // stores the rule inline instead of behind a std::function.
    auto model = vd::make_static_model<std::optional<std::int32_t>>().with(vd::monadic::not_empty);
    static_assert(sizeof(model) == 1);
    SUCCEED();
}

TEST(MonadicNotEmptyStaticModelTest, StaticsMemberCheckerAcceptsEngagedOptional)
{
    auto model = vd::make_static_model<UserSettings>().with(vd::statics::member(&UserSettings::nickname, vd::monadic::not_empty));

    EXPECT_TRUE(model.check(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_FALSE(model.check(UserSettings { std::nullopt, 8080 }));
}

TEST(MonadicNotEmptyStaticModelTest, StaticsFieldCheckerViaGetter)
{
    auto model =
        vd::make_static_model<UserSettings>().with(vd::statics::field("nickname", &UserSettings::get_nickname, vd::monadic::not_empty));

    EXPECT_TRUE(model.check(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_FALSE(model.check(UserSettings { std::nullopt, 8080 }));
}

TEST(MonadicNotEmptyStaticModelTest, RuleFactoryMemberAlsoWorks)
{
    // vd::member yields a vd::rule<T>, which static_model accepts too — the
    // allocating and non-allocating factories must agree on the verdict.
    auto rule_model = vd::make_static_model<UserSettings>().with(vd::member(&UserSettings::nickname, vd::monadic::not_empty));
    auto raw_model = vd::make_static_model<UserSettings>().with(vd::statics::member(&UserSettings::nickname, vd::monadic::not_empty));

    for(const UserSettings& s : { UserSettings { std::string { "ada" }, 1 }, UserSettings { std::nullopt, 1 } }) {
        EXPECT_EQ(rule_model.check(s).is_valid, raw_model.check(s).is_valid);
    }
}

TEST(MonadicNotEmptyStaticModelTest, CheckAggregatesEveryEmptyOptional)
{
    auto model = vd::make_static_model<UserSettings>()
                     .with(vd::statics::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                     .with(vd::statics::member("port", &UserSettings::port, vd::monadic::not_empty));

    vd::result res = model.check(UserSettings { std::nullopt, std::nullopt });

    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.failed_rules.size(), 2u);
}

TEST(MonadicNotEmptyStaticModelTest, ShortCheckStopsAtFirstEmptyOptional)
{
    auto model = vd::make_static_model<UserSettings>()
                     .with(vd::statics::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                     .with(vd::statics::member("port", &UserSettings::port, vd::monadic::not_empty));

    vd::result res = model.short_check(UserSettings { std::nullopt, std::nullopt });

    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("nickname"), std::string::npos);
}

TEST(MonadicNotEmptyStaticModelTest, MixesWithNumericRules)
{
    // A monadic rule and a numeric rule side by side in one static_model — both
    // are now shared stateless objects with a templated operator().
    auto model = vd::make_static_model<Telemetry>()
                     .with(vd::statics::member("sample", &Telemetry::sample, vd::monadic::not_empty))
                     .with(vd::statics::member("threshold", &Telemetry::threshold, vd::numeric::finite_t));

    EXPECT_TRUE(model.check(Telemetry { 1.0, 0.5 }));
    EXPECT_FALSE(model.check(Telemetry { std::nullopt, 0.5 }));
    EXPECT_FALSE(model.check(Telemetry { 1.0, std::numeric_limits<double>::quiet_NaN() }));
    EXPECT_EQ(model.check(Telemetry { std::nullopt, std::numeric_limits<double>::quiet_NaN() }).failed_rules.size(), 2u);
}

TEST(MonadicNotEmptyStaticModelTest, DieIfFailedThrowsOnEmptyOptional)
{
    auto model = vd::make_static_model<UserSettings>().with(vd::statics::member(&UserSettings::nickname, vd::monadic::not_empty));

    EXPECT_NO_THROW(model.die_if_failed(UserSettings { std::string { "ada" }, 8080 }));
    EXPECT_THROW(model.die_if_failed(UserSettings { std::nullopt, 8080 }), vd::validation_exception);
}

TEST(MonadicNotEmptyStaticModelTest, ValidateManyRejectsBatchWithEmptyOptional)
{
    auto model = vd::make_static_model<UserSettings>().with(vd::statics::member(&UserSettings::nickname, vd::monadic::not_empty));

    EXPECT_TRUE(vd::validate_many(model, UserSettings { std::string { "a" }, 1 }, UserSettings { std::string { "b" }, 2 }));
    EXPECT_FALSE(vd::validate_many(model, UserSettings { std::string { "a" }, 1 }, UserSettings { std::nullopt, 2 }));
}

TEST(MonadicNotEmptyStaticModelTest, WithExtensionPreservesBaseModel)
{
    auto base =
        vd::make_static_model<UserSettings>().with(vd::statics::member("nickname", &UserSettings::nickname, vd::monadic::not_empty));
    auto extended = base.with(vd::statics::member("port", &UserSettings::port, vd::monadic::not_empty));

    // base only checks the nickname — a missing port must still pass it
    EXPECT_TRUE(base.check(UserSettings { std::string { "ada" }, std::nullopt }));
    EXPECT_FALSE(extended.check(UserSettings { std::string { "ada" }, std::nullopt }));
    EXPECT_TRUE(extended.check(UserSettings { std::string { "ada" }, 8080 }));
}

// ---------------------------------------------------------------------------
// MonadicAsExpectedTest — std::expected support
//
// The whole vd::monadic::as_expected declaration is gated behind
// __cpp_lib_expected, so every test below is compiled only when the standard
// library in use actually provides std::expected.
//
// Like not_empty, as_expected is one shared object; both T and E are deduced
// from the argument, so neither has to be spelled out at the call site.
// ---------------------------------------------------------------------------

#if defined(__cpp_lib_expected)

struct ParseOutcome {
    std::expected<std::int32_t, std::string> parsed;

    const std::expected<std::int32_t, std::string>& get_parsed() const
    {
        return parsed;
    }
};

using parsed_t = std::expected<std::int32_t, std::string>;
using void_parsed_t = std::expected<void, std::string>;

static parsed_t make_error()
{
    return parsed_t { std::unexpected(std::string { "bad input" }) };
}

TEST(MonadicAsExpectedTest, ValueStatePasses)
{
    EXPECT_TRUE(vd::monadic::as_expected(parsed_t { 42 }));
}

TEST(MonadicAsExpectedTest, ErrorStateFails)
{
    EXPECT_FALSE(vd::monadic::as_expected(make_error()));
}

TEST(MonadicAsExpectedTest, FailureCarriesDescription)
{
    vd::result res = vd::monadic::as_expected(make_error());

    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("unexpected"), std::string::npos);
}

// Presence of a value, not its truthiness — a zero payload is still a value.
TEST(MonadicAsExpectedTest, ValueStateWithZeroPasses)
{
    EXPECT_TRUE(vd::monadic::as_expected(parsed_t { 0 }));
}

TEST(MonadicAsExpectedTest, VoidValueTypeIsSupported)
{
    EXPECT_TRUE(vd::monadic::as_expected(void_parsed_t {}));
    EXPECT_FALSE(vd::monadic::as_expected(void_parsed_t { std::unexpected(std::string { "boom" }) }));
}

// Both T and E are deduced, so one object covers every expected specialization.
TEST(MonadicAsExpectedTest, OneObjectServesEveryTAndE)
{
    using code_t = std::expected<std::string, std::int32_t>;

    EXPECT_TRUE(vd::monadic::as_expected(parsed_t { 1 }));
    EXPECT_TRUE(vd::monadic::as_expected(code_t { std::string { "ok" } }));
    EXPECT_TRUE(vd::monadic::as_expected(void_parsed_t {}));
    EXPECT_FALSE(vd::monadic::as_expected(code_t { std::unexpected(std::int32_t { -1 }) }));
}

TEST(MonadicAsExpectedTest, SatisfiesBothRuleConcepts)
{
    using rule_t = vd::monadic::as_expected_t;
    static_assert(vd::value_checker<rule_t, parsed_t>);
    static_assert(vd::static_rule_for<rule_t, parsed_t>);
    static_assert(vd::value_checker<const rule_t&, parsed_t>);
    static_assert(std::is_same_v<std::invoke_result_t<rule_t, const parsed_t&>, vd::result>);
    static_assert(std::is_empty_v<rule_t>);
    SUCCEED();
}

// The two rules do not accept each other's wrapper — deduction keeps them apart.
TEST(MonadicAsExpectedTest, RulesDoNotCrossAcceptWrappers)
{
    static_assert(!vd::value_checker<vd::monadic::as_expected_t, std::optional<std::int32_t>>);
    static_assert(!vd::value_checker<vd::monadic::not_empty_t, parsed_t>);
    SUCCEED();
}

TEST(MonadicAsExpectedTest, BasicModelMemberChecker)
{
    auto model = vd::basic_model<ParseOutcome> {}.with(vd::member("parsed", &ParseOutcome::parsed, vd::monadic::as_expected));

    EXPECT_TRUE(model.check(ParseOutcome { parsed_t { 42 } }));

    vd::result res = model.check(ParseOutcome { make_error() });
    ASSERT_FALSE(res.is_valid);
    ASSERT_EQ(res.failed_rules.size(), 1u);
    EXPECT_NE(res.failed_rules[0].find("parsed"), std::string::npos);
}

TEST(MonadicAsExpectedTest, BasicModelFieldCheckerViaGetter)
{
    auto model = vd::basic_model<ParseOutcome> {}.with(vd::field("parsed", &ParseOutcome::get_parsed, vd::monadic::as_expected));

    EXPECT_TRUE(model.check(ParseOutcome { parsed_t { 42 } }));
    EXPECT_FALSE(model.check(ParseOutcome { make_error() }));
}

TEST(MonadicAsExpectedTest, BasicModelTopLevelRuleViaExplicitWrap)
{
    auto model = vd::basic_model<parsed_t> {}.with(vd::rule<parsed_t>(vd::monadic::as_expected));

    EXPECT_TRUE(model.check(parsed_t { 42 }));
    EXPECT_FALSE(model.check(make_error()));
}

TEST(MonadicAsExpectedTest, StaticModelTopLevelRule)
{
    auto model = vd::make_static_model<parsed_t>().with(vd::monadic::as_expected);

    EXPECT_TRUE(model.check(parsed_t { 42 }));
    EXPECT_FALSE(model.check(make_error()));
}

TEST(MonadicAsExpectedTest, StaticModelIsConstexprConstructible)
{
    constexpr auto model = vd::make_static_model<parsed_t>().with(vd::monadic::as_expected);

    EXPECT_TRUE(model.check(parsed_t { 42 }));
    EXPECT_FALSE(model.check(make_error()));
}

TEST(MonadicAsExpectedTest, StaticModelStaticsMemberChecker)
{
    auto model = vd::make_static_model<ParseOutcome>().with(vd::statics::member("parsed", &ParseOutcome::parsed, vd::monadic::as_expected));

    EXPECT_TRUE(model.check(ParseOutcome { parsed_t { 42 } }));
    EXPECT_FALSE(model.check(ParseOutcome { make_error() }));
}

TEST(MonadicAsExpectedTest, DieIfFailedThrowsOnErrorState)
{
    auto model = vd::make_static_model<ParseOutcome>().with(vd::statics::member(&ParseOutcome::parsed, vd::monadic::as_expected));

    EXPECT_NO_THROW(model.die_if_failed(ParseOutcome { parsed_t { 42 } }));
    EXPECT_THROW(model.die_if_failed(ParseOutcome { make_error() }), vd::validation_exception);
}

TEST(MonadicAsExpectedTest, CombinesWithOptionalRuleInOneModel)
{
    struct Mixed {
        std::optional<std::string> tag;
        parsed_t parsed;
    };

    auto model = vd::make_static_model<Mixed>()
                     .with(vd::statics::member("tag", &Mixed::tag, vd::monadic::not_empty))
                     .with(vd::statics::member("parsed", &Mixed::parsed, vd::monadic::as_expected));

    EXPECT_TRUE(model.check(Mixed { std::string { "t" }, parsed_t { 1 } }));
    EXPECT_EQ(model.check(Mixed { std::nullopt, make_error() }).failed_rules.size(), 2u);
}

#endif // __cpp_lib_expected
