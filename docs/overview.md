# Validate! — library overview

## What is this

**Validate!** is a header-first C++20 library for declarative data validation.
The idea: describe the validation rules for an object once, through a *model*, and then apply it to any number of objects.

The library is not fully header-only: most of the API is template code in headers, but the non-template parts (`vd::detail::assert_fail`, `vd::result`, `regex_checker`/`regex()` from `string_rules`) are moved into `.cxx` files and built into a static library `vd` (`add_library(vd STATIC ...)` in `CMakeLists.txt`). When consumed via `FetchContent`, linking (`target_link_libraries(... Validate::vd)`) is required — just including `<vd.hxx>` without linking is not enough.

```cpp
struct User {
    std::string name;
    std::string email;
    int age;
    double salary;
};

auto user_model = vd::basic_model<User>()
    .with(vd::member(&User::name,   vd::string_rules::non_empty()))
    .with(vd::member(&User::email,  vd::string_rules::email_like()))
    .with(vd::member(&User::age,    vd::int_bounds::inclusive(18, 120)))
    .with(vd::member(&User::salary, vd::double_bounds::greater_than(0.0)));

User u{"Alice", "alice@example.com", 30, 75000.0};
vd::result ok = user_model.check(u);   // ok.is_valid == true
```

## Requirements

- C++20 (MSVC 19.29+, GCC 13+, Clang 14+)
- CMake 3.23+
- For tests: GTest (fetched automatically via FetchContent)

## Build & test

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## File structure

```
src/
├── vd.hxx                     # The single public header
├── vd_assert.hxx              # re-export: assert module
├── vd_models.hxx              # re-export: all model modules
├── vd_core.hxx                # re-export: vd_result, vd_exception, vd_defines; vd_macro conditionally
├── vd_ext.hxx                 # re-export: Qt extensions (VD_ENABLE_EXTENSION_QT_BASE / _QT_WIDGETS / _QT_QML)
│
├── assert/
│   ├── vd_assert.hxx          # Implementation of vd::require / vd::require_callback / vd::ct_require
│   └── vd_assert.cxx          # Implementation of vd::details::assert_fail
│
├── core/
│   ├── vd_result.hxx          # vd::result — declaration (data + method signatures)
│   ├── vd_result.cxx          # vd::result — method implementations (format/also/die_if_failed/…)
│   ├── vd_exception.hxx       # vd::validation_exception, vd::assertion_exception, vd_tagged_exception<>
│   ├── vd_not_null.hxx        # vd::not_null<T*> — non-null pointer contract
│   ├── vd_defines.hxx         # Attribute macros: VD_LIKELY, VD_NODISCARD, etc.
│   ├── vd_always_false.hxx    # Helper trait always_false<T> for static_assert
│   └── vd_macro.hxx           # Macros VD_MEMBER / VD_FIELD (only under VD_EXPORT_UNSAFE)
│
├── models/
│   ├── vd_rule.hxx            # rule<T>, value_checker concept, member_class_t trait
│   ├── vd_rule_factory.hxx    # Factories: vd::field(), vd::member(), vd::predicate(); vd::statics::field/member
│   ├── vd_basic_model.hxx     # basic_model<T>, basic_bound_model<T>, validate_many()
│   ├── vd_static_model.hxx    # static_model<T, Rules...> — compile-time model (see static-model.md)
│   ├── vd_memory.hxx          # vd::memory::not_null — ready-made checker for pointer-like fields
│   ├── vd_monadic_rules.hxx   # vd::monadic::not_empty / as_expected — optional and expected checkers
│   ├── vd_numeric.hxx         # numeric_bounds<T>, finite_guard + type aliases
│   ├── vd_string_rules.hxx    # string_match<>, regex_checker, length rules, factory functions
│   └── vd_string_rules.cxx    # Implementation of regex_checker::operator() and regex() (std::regex — non-template)
│
├── utils/
│   ├── vd_ctnextafter.hxx     # constexpr ct_nextafter<T> + concept generic_numer
│   ├── vd_overload.hxx        # vd::overloaded<Ts...> — helper for std::visit; unused elsewhere in the library
│   └── vd_sourceloc.hxx       # vd::here() — consteval wrapper around source_location::current(); not called anywhere
│
└── inline_deps/
    └── ctre.hpp               # Compile-Time Regular Expressions (CTRE)

ext/qt/
├── vd_qtbase.hxx              # Aggregating header: qtbase submodules
├── vd_qtwidgets.hxx           # Reserved (empty)
├── vd_qml.hxx                 # Reserved (empty)
└── qtbase/
    ├── vd_qstring.hxx         # qstring_match<>, qregex_checker, length rules, factories
    ├── vd_qstring.cxx         # Implementation of Qt string checkers
    └── vd_qproperty.hxx       # qt_property() for Q_PROPERTY

tests/
├── test_assert.cxx            # Tests for vd::require / vd::ct_require
├── test_models.cxx            # Tests for rule, basic_model, numeric_bounds, finite_guard
├── test_static_model.cxx      # Tests for static_model<T, Rules...>
├── test_string_rules.cxx      # Tests for string_rules, including CharT generalization and length rules
├── test_monadic_rules.cxx     # Tests for vd::monadic against basic_model and static_model
├── test_not_null.cxx          # Tests for vd::not_null<T*>
├── test_integration.cxx       # Integration tests for the whole library
├── test_qt_property.cxx       # Tests for qt_property()
├── test_qt_qstring_rules.cxx  # Tests for vd::qt::string_rules
└── test_qt_integration.cxx    # Integration tests for Qt extensions

docs/
├── overview.md                # This file
├── assert.md                  # Assert module
├── models.md                  # rule, basic_model, basic_bound_model
├── static-model.md            # static_model<T, Rules...> — compile-time model
├── not_null.md                # not_null<T*> and vd::memory::not_null
├── numeric.md                 # numeric_bounds
├── string-rules.md            # string_rules
├── monadic.md                 # vd::monadic — optional / expected checkers
├── extending.md                # How to add new checkers and modules
└── qt extensions.md           # Qt extensions (QString, QProperty)
```

## Key concepts

| Concept | What it is |
|---------|---------|
| `rule<T>` | A single predicate for an object of type `T`. Holds a type-erased callable `const T& -> vd::result`. |
| `basic_model<T>` | A collection of `rule<T>` in a `std::vector` (runtime). `check()` runs all rules and collects errors; `short_check()` is the fail-fast variant, stopping at the first error. |
| `basic_bound_model<T>` | A binding of `basic_model` to a specific object. |
| `static_model<T, Rules...>` | Compile-time model: the set of rules is part of the type (`std::tuple<Rules...>`), not the value. `.with()` returns a new type. See [static-model.md](static-model.md). There is no `basic_bound_model` equivalent for it. |
| `vd::result` | Validation result type: `bool is_valid` + `vector<string> failed_rules`. Converts to `bool`. |
| `vd::not_null<T*>` | Wrapper around a raw pointer: guarantees the value is never `nullptr`. Intended for use in function signatures. Not to be confused with `vd::memory::not_null` — a ready-made checker for fields inside a model. |
| `value_checker` | Concept: callable `V -> bool` (or `V -> vd::result`, since it converts to `bool`). Used as the second argument to the `field`/`member` factories — validates a **field's value**. |
| `static_rule_for<Rule, T>` | Concept for `static_model::with()`: callable `const T& -> bool \| vd::result`. Structurally similar to `value_checker`, but validates the **whole object**, not a field's value. |
| `numeric_bounds<T>` | Implements `value_checker` for numeric types. |
| `finite_guard` | Implements `value_checker` for numeric types by rejecting `NaN`/`inf`. Templates `operator()`, not the class, so the single `vd::numeric::finite_t` object serves every arithmetic type. |
| `string_match<Matcher>` | Implements `value_checker` for strings via an NTTP matcher; generalized over `basic_string_view<CharT>`/`basic_string<CharT>`. |
| `regex_checker` | Implements `value_checker` for strings via `std::regex` with a runtime pattern (`std::string_view` only). |
| `vd::monadic::not_empty<T>` / `as_expected<T, E>` | Factories returning `value_checker`s for `std::optional<T>` / `std::expected<T, E>`. Check engagement only — not the payload. `as_expected` requires C++23. See [monadic.md](monadic.md). |

## How it all connects

```
vd::rule<T>
    ← created via: vd::field(), vd::member(), vd::predicate()
    ← or directly: rule<T>([](const T&) { return vd::result::ok(); })

vd::basic_model<T>
    ← contains: std::vector<rule<T>>
    ← built via: .with(rule), .with({rules...}), .with(other_model)
    ← invokes: check(const T&) / check(const T*) → vd::result (all rules, all errors)
    ←           short_check(const T&)              → vd::result (fail-fast: first error)

vd::static_model<T, Rules...>
    ← contains: std::tuple<Rules...> (rules are part of the type, not the value)
    ← built via: make_static_model<T>().with(rule) — each .with() changes the model's type
    ← rules: vd::field/member/predicate (rule<T>), vd::statics::field/member (no std::function), raw lambdas
    ← check() / short_check() — same semantics as basic_model, but via fold expressions over the tuple

vd::result
    ← returned from: check(), short_check(), rule::operator()(), checkers
    ← methods: format(), short_format(), die_if_failed(), operator bool()

value_checker (concept)
    ← satisfied by: numeric_bounds<T>, string_match<Matcher>, regex_checker, lambdas
    ← used by: factory functions field() / member() as the second argument
```

## Inclusion

The entire public API is available through a single header:

```cpp
#include <vd.hxx>
```
