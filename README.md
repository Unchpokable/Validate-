# Validate!

A C++20 library for declarative data validation. Define validation rules once as a *model*, then apply it to any number of objects. Ships with modern type-safe assertion utilities as a drop-in replacement for the C `assert()` macro.

```cpp
#include <vd.hxx>

struct User {
    std::string name;
    std::string email;
    int         age;
    double      salary;
};

auto user_model = vd::basic_model<User>()
    .with(vd::member(&User::name,   vd::string_rules::non_empty()))
    .with(vd::member(&User::email,  vd::string_rules::email_like()))
    .with(vd::member(&User::age,    vd::int_bounds::inclusive(18, 120)))
    .with(vd::member(&User::salary, vd::double_bounds::greater_than(0.0)));

User u{"Alice", "alice@example.com", 30, 75000.0};
vd::result r = user_model.check(u);  // r.is_valid == true
```

## Features

- **Declarative models** — compose validation rules for any struct or class using a fluent builder API
- **Compile-time models** — `vd::static_model<T, Rules...>` bakes the rule set into the type itself for a heap-alloc-free validation pipeline, alongside the runtime `vd::basic_model<T>`
- **Rich results** — `vd::result` carries all failed rule messages, not just a `bool`
- **Type-safe rule factories** — `vd::member`, `vd::field`, `vd::predicate` (and `vd::statics::member`/`vd::statics::field` for `static_model`) with full template argument deduction; optional field names in error messages
- **Numeric bounds** — inclusive/exclusive ranges, one-sided bounds, and outside-range checks for all arithmetic types
- **String checkers** — compile-time patterns via [CTRE](https://github.com/hanickadot/compile-time-regular-expressions), runtime `std::regex`, length checks (`min_length`, `max_length`, `length_in_between`), and common presets (`email_like`, `uri_like`, `non_empty`, …); most checkers work with any `std::basic_string_view<CharT>` / `std::basic_string<CharT>`, not just `std::string`
- **Modern assertions** — `vd::require` with `std::format` messages, source-location diagnostics, optional exception throwing, and custom callbacks
- **Non-null pointer contract** — `vd::not_null<T*>` enforces that a raw pointer parameter is never `nullptr`, checked at compile time or runtime; `vd::memory::not_null` is the model-rule counterpart for pointer-like fields
- **Qt extension** — `QString` / `QStringView` checkers (including length checks) and `Q_PROPERTY` validation for Qt 5/6 projects

## Requirements

| Toolchain | Minimum version |
|-----------|----------------|
| MSVC      | 19.29 (VS 2019 16.10) |
| GCC       | 13 |
| Clang     | 14 |
| CMake     | 3.23 |
| C++ standard | C++20 |

## Integration via FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    Validate
    GIT_REPOSITORY https://github.com/Unchpokable/Validate.git
    GIT_TAG        main  # or a specific tag/commit
)

FetchContent_MakeAvailable(Validate)

target_link_libraries(my_target PRIVATE Validate::vd)
```

Then include the single public header:

```cpp
#include <vd.hxx>
```

### Qt extension

To enable the Qt Base extension, set the CMake option before calling `FetchContent_MakeAvailable`:

```cmake
set(VD_EXTENSION_QT_BASE ON CACHE BOOL "" FORCE)
# Optional: point to your Qt installation
set(VD_QT_DIR "C:/Qt/6.x.x/msvc2019_64" CACHE PATH "" FORCE)

FetchContent_MakeAvailable(Validate)
```

Qt extensions are pulled in automatically through `<vd.hxx>` when the corresponding `VD_ENABLE_EXTENSION_*` macro is defined by CMake — no extra `#include` needed.

Link `Qt::Core` to your target as usual:

```cmake
target_link_libraries(my_target PRIVATE Validate::vd Qt6::Core)
```

## Building and running tests

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests are only added when Validate! is the top-level CMake project. GTest is fetched automatically via `FetchContent`.

## API overview

### `vd::result`

All validation calls return `vd::result` — a value that carries both the pass/fail status and the list of failure messages:

```cpp
struct result {
    bool is_valid;
    std::vector<std::string> failed_rules;

    operator bool() const;            // if (result) { … }
    std::string format() const;       // numbered multiline report
    std::string short_format() const; // comma-separated single line
    void die_if_failed() const;       // throws vd::validation_exception if not valid

    static result ok();
    static result failed(std::vector<std::string> messages);
};
```

### Models

The core abstraction is `vd::basic_model<T>` — a collection of `vd::rule<T>` predicates.

`check()` runs **all** rules and aggregates every failure message into the returned `vd::result`; it does not stop at the first failure. `short_check()` is the fail-fast variant — it stops on the first failing rule and is cheaper when only a pass/fail answer is needed.

```cpp
// Build a model
auto model = vd::basic_model<Point>()
    .with(vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
    .with(vd::member("y", &Point::y, vd::double_bounds::inclusive(0.0, 100.0)))
    .with(vd::predicate([](const Point& p) { return p.x != p.y; }));

// Validate — all rules run, all failures collected
vd::result r = model.check(point);     // by reference
vd::result r = model.check(&point);    // by pointer (nullptr → result(false))

if (!r) {
    std::cerr << r.format();           // print all failure reasons
}

// Fail-fast — stops on first failure
vd::result r = model.short_check(point);

// Throw vd::validation_exception on failure
model.die_if_failed(point);

// Bind model to an object for repeated checks
auto bound = model.bind(point);
vd::result r = bound.check();
```

**Rule factories:**

| Factory | Use case |
|---------|----------|
| `vd::member("name", &T::field, checker)` | Direct field access with named error context |
| `vd::member(&T::field, checker)` | Direct field access |
| `vd::field("name", &T::getter, checker)` | Const member function with named error context |
| `vd::field(&T::getter, checker)` | Const member function |
| `vd::predicate([](const T&) { … })` | Arbitrary predicate |
| `vd::rule<T>([](const T&) { … })` | Explicit construction |

Any callable satisfying `V → bool` or `V → vd::result` is a valid checker (`value_checker` concept).

**Validating multiple objects:**

```cpp
bool all_ok = vd::validate_many(model, a, b, c);             // variadic
bool all_ok = vd::validate_many(model, vec_of_objects);       // std::vector<T>
bool all_ok = vd::validate_many(model, vec_of_pointers);      // std::vector<T*>
```

### Static models: `vd::static_model<T, Rules...>`

`vd::static_model<T, Rules...>` is a compile-time alternative to `basic_model<T>`: the rule set is baked into the model's *type* (a `std::tuple<Rules...>`) instead of stored in a runtime `std::vector`. Combined with `vd::statics::member`/`vd::statics::field` (which return raw lambdas instead of `rule<T>`), it builds a validation pipeline without any `std::function` heap allocations.

```cpp
auto model = vd::make_static_model<Point>()
    .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
    .with(vd::statics::member(&Point::y, vd::double_bounds::inclusive(0.0, 100.0)))
    .with([](const Point& p) { return p.x != p.y; });   // raw predicate lambdas work too

vd::result r = model.check(point);      // same check()/short_check()/die_if_failed() contract as basic_model
```

Every `.with()` call returns a *new* `static_model<T, Rules..., NewRule>` — the original model is left untouched, since its rule set can't change without changing its type. There is no bound-model equivalent (`basic_bound_model`) and no `add_rule()` for `static_model`. Use `basic_model` when the rule set needs to be assembled at runtime or stored uniformly regardless of rule count; use `static_model` when the full rule set is known at compile time and allocation-free validation matters. See [docs/static-model.md](docs/static-model.md) for the full API.

### `vd::not_null<T*>`

A lightweight contract wrapper that enforces a raw pointer is never `nullptr`. Intended as a function parameter type to express the non-null precondition in the signature itself.

```cpp
void process(vd::not_null<Widget*> w) {
    w->do_something();  // safe to dereference, contract checked at call site
}

Widget* ptr = get_widget();
process(ptr);           // compiles — checked at runtime, terminates if nullptr
process(nullptr);       // does not compile — nullptr_t overload is deleted
```

Construction from a null pointer in a `constexpr` context is a compile error; at runtime it calls `std::terminate()` with a diagnostic message on `stderr`.

`not_null<T*>` supports `*`, `->`, implicit conversion to `T*`, and comparison operators. It does not transfer ownership.

### Numeric bounds

```cpp
vd::int_bounds::inclusive(0, 100)               // [0, 100]
vd::int_bounds::exclusive(0, 100)               // (0, 100)  — works for integers too
vd::double_bounds::greater_than(0.0)            // (0, +∞)
vd::float_bounds::less_than(1.0f)               // (-∞, 1)
vd::long_bounds::unbounded()                    // always true
vd::double_bounds::outside_inclusive(1.0, 9.0)  // ≤ 1.0 or ≥ 9.0
vd::double_bounds::outside_exclusive(1.0, 9.0)  // < 1.0 or > 9.0
```

Exclusive bounds use the library's own `vd::ct_nextafter<T>` — a `constexpr` implementation of `std::nextafter` that works with both floating-point and integer types.

Check for finite values (rejects `NaN` and `inf`):

```cpp
vd::numeric::finite<double>()
```

Available aliases: `byte_bounds`, `short_bounds`, `int_bounds`, `long_bounds`, `float_bounds`, `double_bounds`, and their unsigned / signed variants.

### String rules

```cpp
vd::string_rules::non_empty()                    // length > 0
vd::string_rules::empty()                        // length == 0
vd::string_rules::empty_or_whitespace()           // empty or ASCII whitespace only
vd::string_rules::email_like()                    // ^\S+@\S+\.\S+$ via CTRE
vd::string_rules::uri_like()                      // ^\w+://\S+$ via CTRE
vd::string_rules::regex(R"(\d{5})")               // runtime std::regex (full-string match)
vd::string_rules::min_length(3)                   // length (code units) >= 3
vd::string_rules::max_length(20)                  // length (code units) <= 20
vd::string_rules::length_in_between(3, 20)        // 3 <= length <= 20
```

All checkers accept `std::string_view` and return `vd::result`. `std::string` fields work without explicit conversion.

`non_empty`, `empty`, `empty_or_whitespace`, and the three length checkers additionally accept any `std::basic_string_view<CharT>` / `std::basic_string<CharT>` (`wchar_t`, `char8_t`, `char16_t`, `char32_t`), so they work with `std::wstring`, `std::u16string`, etc. `email_like`, `uri_like`, and `regex` remain `std::string_view`-only. Length checkers count **code units**, not user-perceived characters — see [docs/string-rules.md](docs/string-rules.md) for the distinction. `min_length`/`max_length`/`length_in_between` throw `vd::assertion_exception` if constructed with invalid bounds (e.g. `max_length(0)` or `length_in_between(10, 5)`).

### `vd::require`

`vd::require` is a type-safe, source-aware replacement for `assert()`.

```cpp
// Abort on failure — prints file, line, function to stderr
vd::require(ptr != nullptr, "Expected non-null pointer in {}", __func__);

// Throw on failure — exception.what() contains the formatted message only
vd::require<std::runtime_error>(value > 0, "Value must be positive, got {}", value);

// Custom callback on failure (zero-overhead NTTP, receives std::string_view)
void my_logger(std::string_view msg) { /* … */ }
vd::require_callback<my_logger>(ok, "Validation failed: {}", reason);
```

The format string is checked at compile time via `std::format_string`. Source location is captured at the call site — diagnostics always point to your code, not library internals.

**Output on abort:**
```
Assertion failed: Expected non-null pointer in foo
File: src/foo.cpp
Line: 42
Function: void foo()
```

**Debug-only overloads** (`required`, `require_callbackd`) compile to no-ops when `_NDEBUG` is defined.

### Qt extension

Drop-in counterparts for Qt types:

```cpp
#include <vd.hxx>  // Qt extensions included automatically when VD_EXTENSION_QT_BASE=ON

// QString checkers (mirror of vd::string_rules, operate on QStringView)
vd::qt::string_rules::non_empty()
vd::qt::string_rules::email_like()
vd::qt::string_rules::regex(R"(\+7\d{10})")  // uses QRegularExpression / PCRE2
vd::qt::string_rules::min_length(3)
vd::qt::string_rules::max_length(280)        // counts UTF-16 code units — not portable 1:1 with the std-side byte count

// Q_PROPERTY validation
auto model = vd::basic_model<MyQObject>()
    .with(vd::qt::qt_property<MyQObject>("email", vd::qt::string_rules::email_like()))
    .with(vd::qt::qt_property<MyQObject>("age",   vd::int_bounds::inclusive(18, 120)));
```

`qt_property` reads property values through the Qt meta-object system at validation time. Returns a failed `vd::result` (without abort) if the object is not a `QObject`, the property does not exist, or the value cannot be converted to the expected type.

The Qt `empty_or_whitespace()` checker uses `QChar::isSpace()` and covers all Unicode whitespace, unlike the std version which checks ASCII only.

## Extending the library

Any callable `V → bool` or `V → vd::result` is a valid checker — no inheritance or registration required. Returning `vd::result` is preferred as it carries the failure reason:

```cpp
// Lambda (bool return)
vd::member(&Product::price, [](double v) { return v > 0 && v < 1e6; })

// Stateful checker struct with detailed error message
struct min_length {
    std::size_t n;
    vd::result operator()(std::string_view s) const {
        if (s.size() >= n) return vd::result::ok();
        return vd::result::failed({std::format("string length {} < minimum {}", s.size(), n)});
    }
};
vd::member(&Post::body, min_length{10})

// Stateless function for use with string_match (supports include/exclude mode)
namespace my_rules {
    inline bool is_hex_color(std::string_view s) {
        if (s.size() != 7 || s[0] != '#') return false;
        for (int i = 1; i < 7; ++i)
            if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }
    inline auto hex_color() {
        return vd::string_rules::string_match<is_hex_color>{
            vd::string_rules::string_match<is_hex_color>::mode::include
        };
    }
}
```

## Further reading

The [`docs/`](docs/) directory covers the internals — design rationale, TMP/concept choices, and module-by-module reference: [overview](docs/overview.md), [models](docs/models.md), [static-model](docs/static-model.md), [numeric](docs/numeric.md), [string-rules](docs/string-rules.md), [assert](docs/assert.md), [not_null](docs/not_null.md), [qt extensions](docs/qt%20extensions.md), [extending](docs/extending.md).

## License

MIT — see [LICENSE](LICENSE).
