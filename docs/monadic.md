# Monadic rules module

**Header:** `#include <vd.hxx>`  
**Implementation file:** `src/models/vd_monadic_rules.hxx`  
**Namespace:** `vd::monadic`  
**Requires:** `<optional>` for `not_empty`; `<expected>` (C++23, feature-test gated) for `as_expected`

---

## Purpose

The module provides checkers for the standard monadic wrapper types — `std::optional<T>` and `std::expected<T, E>`. Both answer the same question in their own vocabulary: *does this wrapper actually carry a value?*

```cpp
struct UserSettings {
    std::optional<std::string>  nickname;
    std::optional<std::int32_t> port;
};

auto model = vd::basic_model<UserSettings>()
    .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty<std::string>()))
    .with(vd::member("port",     &UserSettings::port,     vd::monadic::not_empty<std::int32_t>()));
```

Unlike `vd::numeric` and `vd::string_rules`, this module deliberately contains no checkers that look *inside* the wrapper. Composing "is engaged **and** the contents are valid" is left to the caller — see [Composing with a checker for the payload](#composing-with-a-checker-for-the-payload).

---

## `vd::monadic::not_empty<T>()`

```cpp
namespace vd::monadic {
    template<typename T>
    constexpr auto not_empty();   // -> lambda: (const std::optional<T>&) -> vd::result
}
```

Returns a checker that passes when the `std::optional<T>` is engaged.

`T` is the *payload* type, not the optional type — write `not_empty<std::string>()` for a `std::optional<std::string>` field.

### Engagement, not truthiness

The check asks `opt.has_value()`, and nothing else. An engaged optional holding a falsy payload passes:

```cpp
auto rule = vd::monadic::not_empty<std::int32_t>();

rule(std::optional<std::int32_t>{0});       // ok    — engaged
rule(std::optional<std::int32_t>{});        // failed — disengaged
rule(std::nullopt);                          // failed

vd::monadic::not_empty<bool>()(std::optional<bool>{false});          // ok
vd::monadic::not_empty<std::string>()(std::optional<std::string>{""}); // ok
```

This is the distinction that makes the rule composable: "present" and "valid" stay separate concerns, so a payload checker can be added independently without either rule second-guessing the other.

### Failure message

```
Value must not be empty
```

---

## `vd::monadic::as_expected<T, E>()`

```cpp
#if defined(__cpp_lib_expected)
namespace vd::monadic {
    template<typename T, typename E>
    constexpr auto as_expected();   // -> lambda: (const std::expected<T, E>&) -> vd::result
}
#endif
```

Returns a checker that passes when the `std::expected<T, E>` holds a value rather than an error.

Both `T` and `E` must be spelled out — there is nothing in the call to deduce them from:

```cpp
using parsed_t = std::expected<std::int32_t, std::string>;

auto rule = vd::monadic::as_expected<std::int32_t, std::string>();

rule(parsed_t{42});                                 // ok
rule(parsed_t{std::unexpected("bad input")});       // failed
```

`std::expected<void, E>` is supported — a valueless-but-successful expected passes:

```cpp
auto rule = vd::monadic::as_expected<void, std::string>();
rule(std::expected<void, std::string>{});           // ok
```

### Availability

The whole `as_expected` declaration sits behind `#if defined(__cpp_lib_expected)`, tested after including `<version>`. On a toolchain without `<expected>` the symbol simply does not exist rather than failing to compile, so the rest of the module (and the rest of the library) stays usable on a plain C++20 setup.

Guard your own call sites the same way if you target such toolchains:

```cpp
#if defined(__cpp_lib_expected)
    model.with(vd::member(&ParseOutcome::parsed, vd::monadic::as_expected<std::int32_t, std::string>()));
#endif
```

### Failure message

```
Value must not be empty
```

Note that the message is shared with `not_empty` and does not mention the error state or reproduce `E`. If you need the error payload in the diagnostics, write a `vd::predicate` that formats it.

---

## Use with the models

Both factories return plain lambdas returning `vd::result`, so they satisfy `value_checker` and `static_rule_for` and can be used through every factory the library offers.

### As a member/field checker

```cpp
// runtime model
vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty<std::string>())
vd::field("nickname",  &UserSettings::get_nickname, vd::monadic::not_empty<std::string>())

// compile-time model — allocation-free
vd::statics::member("nickname", &UserSettings::nickname, vd::monadic::not_empty<std::string>())
vd::statics::field("nickname",  &UserSettings::get_nickname, vd::monadic::not_empty<std::string>())
```

The named overloads propagate the member name into the failure text:

```
Member nickname failed: Value must not be empty,
```

### As a top-level rule

Unlike `vd::numeric::finite_t`, these checkers are *non-generic* lambdas — their `operator()` has exactly one signature, so `vd::predicate` can deduce the value type from it:

```cpp
auto model = vd::basic_model<std::optional<std::int32_t>>{}
                 .with(vd::predicate(vd::monadic::not_empty<std::int32_t>()));

static_assert(std::is_same_v<decltype(vd::predicate(vd::monadic::not_empty<std::int32_t>())),
                             vd::rule<std::optional<std::int32_t>>>);
```

Note that the deduced type is `std::optional<T>`, not `T` — the model's value type is the optional itself. An explicit wrap does the same thing and is what you need when constructing a model from an initializer list, since `vd::rule`'s converting constructor is `explicit`:

```cpp
vd::basic_model<std::optional<std::int32_t>> model {
    vd::rule<std::optional<std::int32_t>>(vd::monadic::not_empty<std::int32_t>())
};
```

With `static_model` the lambda is passed straight to `.with()` and stored inline:

```cpp
auto model = vd::make_static_model<std::optional<std::int32_t>>()
                 .with(vd::monadic::not_empty<std::int32_t>());
```

Since the returned lambda is captureless and the factories are `constexpr`, such a model is constexpr-constructible.

---

## Composing with a checker for the payload

The module intentionally stops at "is there a value". To also constrain the contents, add a second rule that unwraps the optional itself — this keeps the two failures independently reportable:

```cpp
auto model = vd::basic_model<UserSettings>()
    .with(vd::member("port", &UserSettings::port, vd::monadic::not_empty<std::int32_t>()))
    .with(vd::predicate([](const UserSettings& s) -> vd::result {
        if(!s.port.has_value()) {
            return vd::result::ok();   // absence is the other rule's business
        }
        return vd::int_bounds::inclusive(1, 65535)(*s.port);
    }));
```

Returning `ok()` for the disengaged case is what keeps a single missing value from producing two failure messages. `check()` aggregates every rule, so the alternative would report both "must not be empty" and a bogus range violation; `short_check()` would stop at the first and hide the second.

---

## Adding a checker for your own monadic type

Any type with a "has a value" query fits the same shape — the module's factories are only three lines each. For a custom `Result<T>`:

```cpp
namespace my::rules
{
template<typename T>
constexpr auto engaged()
{
    return [](const Result<T>& r) -> vd::result {
        if(r.is_ok()) {
            return vd::result::ok();
        }
        return vd::result::failed({ std::format("Result is in error state: {}", r.error()) });
    };
}
} // namespace my::rules
```

Returning a non-generic lambda (rather than a functor with a templated `operator()`) is what keeps `vd::predicate` deduction working — see [extending.md](extending.md) for the full checker contract.
