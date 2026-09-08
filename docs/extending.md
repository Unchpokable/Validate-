# Extending the library

This document describes how to add new checkers, new model types, and how to evolve the existing abstractions.

---

## Writing a new checker

There's a single requirement: the type must be callable `V -> bool` or `V -> vd::result` (i.e. satisfy `value_checker<Checker, V>`). No inheritance, macros, or registration needed.

Returning `vd::result` is preferable — it lets you pass error text that ends up in `vd::result::failed_rules`.

### Option 1: lambda

The simplest case. Used right at the point where the rule is created:

```cpp
auto model = vd::basic_model<Product>()
    .with(vd::member(&Product::price, [](double v) { return v > 0 && v < 1e6; }))
    .with(vd::predicate([](const Product& p) { return !p.name.empty() && p.sku > 0; }));
```

### Option 2: a struct with state

When the checker is parameterized by runtime data:

```cpp
struct multiple_of {
    int divisor;
    vd::result operator()(int v) const noexcept {
        if(v % divisor == 0) return vd::result::ok();
        return vd::result::failed({std::format("value {} is not divisible by {}", v, divisor)});
    }
};

struct min_length {
    std::size_t n;
    vd::result operator()(std::string_view s) const noexcept {
        if(s.size() >= n) return vd::result::ok();
        return vd::result::failed({std::format("string length {} < minimum {}", s.size(), n)});
    }
};

auto model = vd::basic_model<Item>()
    .with(vd::member(&Item::quantity, multiple_of{5}))
    .with(vd::member(&Item::label,    min_length{3}));
```

### Option 2b: a stateless checker templated on `operator()`

When the check is the same for a whole family of value types, template `operator()` rather than the struct, and expose one shared object. `vd::numeric::finite_guard` and both `vd::monadic` rules are the library's own examples of this shape:

```cpp
struct finite_guard final {
    template<numeric_compatible T>
    vd::result operator()(const T& value) const;
};

inline constexpr auto finite_t = finite_guard{};

struct not_empty_t final {
    template<typename T>
    vd::result operator()(const std::optional<T>& opt) const;
};

inline constexpr not_empty_t not_empty;
```

Two things this buys you: unsuitable types make `value_checker` fail cleanly instead of hard-erroring inside an instantiation, and an empty checker costs `static_model` no storage at all.

The clean failure comes from one of two mechanisms, and you want exactly one of them:

* a **constraint** on the template parameter, as `finite_guard` does with `numeric_compatible`;
* a **deduced pattern** in the parameter type, as `not_empty_t` does with `const std::optional<T>&` — every non-optional argument is then a deduction failure in the immediate context.

`operator()` must be **`const`-qualified**. The shared object is `inline constexpr` and therefore const, so a non-const `operator()` makes it uncallable — both directly and from inside the const lambdas `vd::member` / `vd::field` build around it.

One trap comes with it: **`vd::predicate` cannot deduce a value type from such a checker.** It goes through `detail::first_arg_of`, defined as `first_arg_of<decltype(&Fn::operator())>`, and a templated `operator()` names an overload set whose address cannot be taken. The resulting error is hard and outside the immediate context, so it is not even detectable with `requires`. Wrap explicitly instead:

```cpp
vd::basic_model<double>{}.with(vd::rule<double>(vd::numeric::finite_t));   // ok
vd::basic_model<double>{}.with(vd::predicate(vd::numeric::finite_t));      // does not compile

vd::basic_model<std::optional<int>>{}
    .with(vd::rule<std::optional<int>>(vd::monadic::not_empty));           // ok
vd::basic_model<std::optional<int>>{}
    .with(vd::predicate(vd::monadic::not_empty));                          // does not compile
```

`static_model` is unaffected — `static_rule_for<Rule, T>` already knows `T`, so nothing has to be deduced from the checker:

```cpp
vd::make_static_model<double>().with(vd::numeric::finite_t);               // ok
vd::make_static_model<std::optional<int>>().with(vd::monadic::not_empty);  // ok
```

Nor are `vd::member` / `vd::field`, which take `T` from the member pointer — so the common case, a checker attached to a field, never runs into this at all.

If your checker only ever handles one value type, prefer a non-generic `operator()` (Option 2) — it keeps `vd::predicate` working. Reach for this option when the alternative would be making callers spell the type out at every call site, which is the trade `vd::monadic` deliberately makes.

### Option 3: `string_match<Matcher>` for stateless string checkers

When the matcher is a stateless function `bool(std::string_view)` and `mode::include/exclude` support is needed:

```cpp
namespace my_rules {
    inline bool is_hex_color(std::string_view s) {
        if(s.size() != 7 || s[0] != '#') return false;
        for(int i = 1; i < 7; ++i)
            if(!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }
}

// Usage:
auto checker = vd::string_rules::string_match<my_rules::is_hex_color>{
    vd::string_rules::string_match<my_rules::is_hex_color>::mode::include
};

// Factory function (recommended for readability):
inline auto hex_color() {
    return vd::string_rules::string_match<my_rules::is_hex_color>{
        vd::string_rules::string_match<my_rules::is_hex_color>::mode::include
    };
}
```

---

## Adding a new checker module

Following the pattern of `vd_numeric.hxx`, `vd_string_rules.hxx` and `vd_monadic_rules.hxx`:

1. Create `src/models/vd_<your_module>.hxx`
2. Add `#include "models/vd_<your_module>.hxx"` to `src/vd_models.hxx`
3. Define checker types in namespace `vd` or `vd::<your_namespace>`
4. All factory functions in the `.hxx` must be `inline` (a `constexpr` function factory, or an `inline constexpr` object for a stateless checker, works too)
5. If the module depends on a library feature that is not universally available, gate it on the feature-test macro rather than on the language standard — `vd_monadic_rules.hxx` includes `<version>` and wraps `as_expected` in `#if defined(__cpp_lib_expected)`, so the rest of the module still compiles on a C++20 toolchain

Template for a new module:

```cpp
#pragma once
#ifndef VD_MY_MODULE_HXX
#define VD_MY_MODULE_HXX

#include "vd_basic_model.hxx"
#include "core/vd_result.hxx"
// ... other headers as needed

namespace vd::my_rules
{
struct my_checker {
    /* parameters */
    vd::result operator()(/* value type */ v) const;
};

inline my_checker some_condition(/* parameters */)
{
    return { /* ... */ };
}
} // namespace vd::my_rules

#endif // VD_MY_MODULE_HXX
```

---

## Extending `basic_model`

`basic_model<T>` is intentionally minimalistic: just a set of rules and `check()`. If you need a specialized model type (e.g. with result caching, named rules, or error aggregation), there's no need to inherit — just write a new type that uses `rule<T>` internally.

If, instead of a specialized model, you just need a heap-alloc-free pipeline with a rule set known at compile time — `vd::static_model<T, Rules...>` already provides that, no custom type needed. See [static-model.md](static-model.md).

---

## Tests

When adding a new module, create a separate test file in `tests/`:

```
tests/test_<module>.cxx
```

Register it in `tests/CMakeLists.txt`:

```cmake
add_executable(test_my_module test_my_module.cxx)
target_link_libraries(test_my_module PRIVATE Validate::vd GTest::gtest_main)
gtest_discover_tests(test_my_module)
```

Testing principle: if a test fails, that's a reason to revisit the implementation, not to adjust the test.
