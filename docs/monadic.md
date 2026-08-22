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
    .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
    .with(vd::member("port",     &UserSettings::port,     vd::monadic::not_empty));
```

Each rule is a single stateless object with a templated `operator()`, so the payload types are deduced from the argument and the same object serves every specialization — `not_empty` covers every `std::optional<T>`, `as_expected` every `std::expected<T, E>`.

Unlike `vd::numeric` and `vd::string_rules`, this module deliberately contains no checkers that look *inside* the wrapper. Composing "is engaged **and** the contents are valid" is left to the caller — see [Composing with a checker for the payload](#composing-with-a-checker-for-the-payload).

---

## `vd::monadic::not_empty`

```cpp
namespace vd::monadic {
    struct not_empty_t final {
        template<typename T>
        vd::result operator()(const std::optional<T>& opt) const;
    };

    inline constexpr not_empty_t not_empty;
}
```

Passes when the `std::optional<T>` is engaged. `T` is deduced from the argument — nothing is spelled out at the call site.

### Engagement, not truthiness

The check asks `opt.has_value()`, and nothing else. An engaged optional holding a falsy payload passes:

```cpp
vd::monadic::not_empty(std::optional<std::int32_t>{0});   // ok     — engaged
vd::monadic::not_empty(std::optional<std::int32_t>{});    // failed — disengaged

vd::monadic::not_empty(std::optional<bool>{false});       // ok
vd::monadic::not_empty(std::optional<std::string>{""});   // ok
```

This is the distinction that makes the rule composable: "present" and "valid" stay separate concerns, so a payload checker can be added independently without either rule second-guessing the other.

### Deduction is the type filter

`T` is deduced from `const std::optional<T>&`, so anything that is not a `std::optional` fails template argument deduction **in the immediate context**. `value_checker` and `static_rule_for` therefore report a clean `false` instead of hard-erroring inside an instantiation:

```cpp
static_assert( vd::value_checker<vd::monadic::not_empty_t, std::optional<std::int32_t>>);
static_assert(!vd::value_checker<vd::monadic::not_empty_t, std::int32_t>);
static_assert(!vd::value_checker<vd::monadic::not_empty_t, std::string>);
```

One consequence worth knowing: a bare `std::nullopt` is **not** accepted, because `T` cannot be deduced from `std::nullopt_t`. Spell the optional out instead:

```cpp
vd::monadic::not_empty(std::nullopt);                     // does not compile
vd::monadic::not_empty(std::optional<std::int32_t>{});    // failed — this is the way
```

### Failure message

```
Value must not be empty!
```

---

## `vd::monadic::as_expected`

```cpp
#if defined(__cpp_lib_expected)
namespace vd::monadic {
    struct as_expected_t final {
        template<typename T, typename E>
        vd::result operator()(const std::expected<T, E>& expected) const;
    };

    inline constexpr as_expected_t as_expected;
}
#endif
```

Passes when the `std::expected<T, E>` holds a value rather than an error. Both `T` and `E` are deduced from the argument, so one object covers every specialization:

```cpp
using parsed_t = std::expected<std::int32_t, std::string>;
using code_t   = std::expected<std::string, std::int32_t>;

vd::monadic::as_expected(parsed_t{42});                             // ok
vd::monadic::as_expected(parsed_t{std::unexpected("bad input")});   // failed
vd::monadic::as_expected(code_t{std::string{"ok"}});                // ok — same object
```

`std::expected<void, E>` is supported — a valueless-but-successful expected passes:

```cpp
using void_parsed_t = std::expected<void, std::string>;
vd::monadic::as_expected(void_parsed_t{});                          // ok
```

The two rules do not accept each other's wrapper; deduction keeps them apart:

```cpp
static_assert(!vd::value_checker<vd::monadic::as_expected_t, std::optional<std::int32_t>>);
static_assert(!vd::value_checker<vd::monadic::not_empty_t,   parsed_t>);
```

### Availability

The whole `as_expected` declaration sits behind `#if defined(__cpp_lib_expected)`, tested after including `<version>`. On a toolchain without `<expected>` the symbol simply does not exist rather than failing to compile, so the rest of the module (and the rest of the library) stays usable on a plain C++20 setup.

Guard your own call sites the same way if you target such toolchains:

```cpp
#if defined(__cpp_lib_expected)
    model.with(vd::member(&ParseOutcome::parsed, vd::monadic::as_expected));
#endif
```

### Failure message

```
Value is unexpected
```

The message does not reproduce `E` or the error payload. If you need it in the diagnostics, write a `vd::predicate` that formats it.

---

## Use with the models

Both objects return `vd::result`, so they satisfy `value_checker` and `static_rule_for`. Because they are `inline constexpr` — and therefore `const` — their `operator()` is const-qualified: that is what lets the rule factories call them from inside the const lambdas they build.

### As a member/field checker

```cpp
// runtime model
vd::member("nickname", &UserSettings::nickname,     vd::monadic::not_empty)
vd::field ("nickname", &UserSettings::get_nickname, vd::monadic::not_empty)

// compile-time model — allocation-free
vd::statics::member("nickname", &UserSettings::nickname,     vd::monadic::not_empty)
vd::statics::field ("nickname", &UserSettings::get_nickname, vd::monadic::not_empty)
```

The named overloads propagate the member name into the failure text:

```
Member nickname failed: Value must not be empty!,
```

Since the payload type is deduced per call, one object covers members of different payload types in the same model:

```cpp
auto model = vd::basic_model<UserSettings>{}
                 .with(vd::member("nickname", &UserSettings::nickname, vd::monadic::not_empty))
                 .with(vd::member("port",     &UserSettings::port,     vd::monadic::not_empty));
```

### As a top-level rule

With `basic_model` the rule has to be wrapped in `vd::rule<T>` explicitly. The wrap is what pins down `T`, which a templated `operator()` cannot supply on its own:

```cpp
auto model = vd::basic_model<std::optional<std::int32_t>>{}
                 .with(vd::rule<std::optional<std::int32_t>>(vd::monadic::not_empty));
```

Note that the value type is `std::optional<T>`, not `T` — the model's value type is the optional itself. The same explicit wrap is what you need when constructing a model from an initializer list, since `vd::rule`'s converting constructor is `explicit`:

```cpp
vd::basic_model<std::optional<std::int32_t>> model {
    vd::rule<std::optional<std::int32_t>>(vd::monadic::not_empty)
};
```

> **`vd::predicate` does not work here.** It deduces `T` through `detail::first_arg_of`, which takes the address of `operator()` — impossible for a templated one, and the resulting error is hard and outside the immediate context. This is the same trade-off `vd::numeric::finite_t` carries; see [extending.md](extending.md#option-2b-a-stateless-checker-templated-on-operator).
>
> ```cpp
> vd::predicate(vd::monadic::not_empty);                        // does not compile
> vd::rule<std::optional<std::int32_t>>(vd::monadic::not_empty); // ok
> ```

With `static_model` the object is passed straight to `.with()` and stored inline — `static_rule_for<Rule, T>` already knows `T`, so nothing has to be deduced from the checker:

```cpp
auto model = vd::make_static_model<std::optional<std::int32_t>>()
                 .with(vd::monadic::not_empty);
```

Both rule types are empty and constexpr-constructible, so such a model is constexpr-constructible and costs no storage:

```cpp
constexpr auto model = vd::make_static_model<std::optional<std::int32_t>>()
                           .with(vd::monadic::not_empty);
static_assert(sizeof(model) == 1);
```

---

## Composing with a checker for the payload

The module intentionally stops at "is there a value". To also constrain the contents, add a second rule that unwraps the optional itself — this keeps the two failures independently reportable:

```cpp
auto model = vd::basic_model<UserSettings>()
    .with(vd::member("port", &UserSettings::port, vd::monadic::not_empty))
    .with(vd::predicate([](const UserSettings& s) -> vd::result {
        if(!s.port.has_value()) {
            return vd::result::ok();   // absence is the other rule's business
        }
        return vd::int_bounds::inclusive(1, 65535)(*s.port);
    }));
```

Returning `ok()` for the disengaged case is what keeps a single missing value from producing two failure messages. `check()` aggregates every rule, so the alternative would report both "must not be empty" and a bogus range violation; `short_check()` would stop at the first and hide the second.

Note that `vd::predicate` is usable here because the argument is a plain non-generic lambda — the restriction above applies only to the `vd::monadic` objects themselves.

---

## Adding a checker for your own monadic type

Any type with a "has a value" query fits the same shape. Follow the module's own pattern — a stateless struct with a templated, `const`-qualified `operator()`, plus one shared `inline constexpr` object. For a custom `Result<T>`:

```cpp
namespace my::rules
{
struct is_ok_t final {
    template<typename T>
    vd::result operator()(const Result<T>& r) const
    {
        if(r.is_ok()) {
            return vd::result::ok();
        }
        return vd::result::failed({ std::format("Result is in error state: {}", r.error()) });
    }
};

inline constexpr is_ok_t is_ok;
} // namespace my::rules
```

Three details are load-bearing:

* **`const` on `operator()`** — the shared object is `constexpr`, hence const. Without it the checker cannot be called at all, neither directly nor from inside the const lambdas the rule factories build.
* **Deduce, don't constrain by hand** — writing the parameter as `const Result<T>&` makes every non-`Result` argument a clean deduction failure, so `value_checker` reports `false` instead of hard-erroring.
* **`vd::predicate` stops working** — a templated `operator()` cannot have its address taken. Prefer a non-generic `operator()` if the checker only ever handles one value type.

See [extending.md](extending.md) for the full checker contract.
