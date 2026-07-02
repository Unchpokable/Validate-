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

Following the pattern of `vd_numeric.hxx` and `vd_string_rules.hxx`:

1. Create `src/models/vd_<your_module>.hxx`
2. Add `#include "models/vd_<your_module>.hxx"` to `src/vd_models.hxx`
3. Define checker types in namespace `vd` or `vd::<your_namespace>`
4. All factory functions in the `.hxx` must be `inline`

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
