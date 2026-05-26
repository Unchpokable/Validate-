# Validate!

A header-only C++20 library for declarative data validation. Define validation rules once as a *model*, then apply it to any number of objects. Ships with modern type-safe assertion utilities as a drop-in replacement for the C `assert()` macro.

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
bool ok = user_model.is_valid(u);  // true
```

## Features

- **Declarative models** — compose validation rules for any struct or class using a fluent builder API
- **Type-safe rule factories** — `vd::member`, `vd::field`, `vd::predicate` with full template argument deduction
- **Numeric bounds** — inclusive/exclusive ranges, one-sided bounds for all arithmetic types
- **String checkers** — compile-time patterns via [CTRE](https://github.com/hanickadot/compile-time-regular-expressions), runtime `std::regex`, and common presets (`email_like`, `uri_like`, `non_empty`, …)
- **Modern assertions** — `vd::require` with `std::format` messages, source-location diagnostics, optional exception throwing, and custom callbacks
- **Qt extension** — `QString` / `QStringView` checkers and `Q_PROPERTY` validation for Qt 6 projects

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

### Models

The core abstraction is `vd::basic_model<T>` — a collection of `vd::rule<T>` predicates applied sequentially by `is_valid()`.

```cpp
// Build a model
auto model = vd::basic_model<Point>()
    .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
    .with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 100.0)))
    .with(vd::predicate([](const Point& p) { return p.x != p.y; }));

// Validate
bool ok = model.is_valid(point);          // by reference
bool ok = model.is_valid(&point);         // by pointer (nullptr → abort)

// Bind model to an object
auto bound = model.bind(point);
bool ok = bound.is_valid();
```

**Rule factories:**

| Factory | Use case |
|---------|----------|
| `vd::member(&T::field, checker)` | Direct field access |
| `vd::field(&T::getter, checker)` | Const member function |
| `vd::predicate([](const T&) { … })` | Arbitrary predicate |
| `vd::rule<T>([](const T&) { … })` | Explicit construction |

Any callable satisfying `V → bool` is a valid checker (`value_checker` concept).

### Numeric bounds

```cpp
vd::int_bounds::inclusive(0, 100)         // [0, 100]
vd::int_bounds::exclusive(0, 100)         // (0, 100) — compile error for int, use float/double
vd::double_bounds::greater_than(0.0)      // (0, +∞)
vd::float_bounds::less_than(1.0f)         // (-∞, 1)
vd::long_bounds::unbounded()              // always true
```

`exclusive`, `greater_than`, and `less_than` use `std::nextafter` internally and are only available for floating-point types. Use `inclusive` with a manual offset for integers.

Available aliases: `byte_bounds`, `short_bounds`, `int_bounds`, `long_bounds`, `float_bounds`, `double_bounds`, and their unsigned variants.

### String rules

```cpp
vd::string_rules::non_empty()              // length > 0
vd::string_rules::empty()                 // length == 0
vd::string_rules::empty_or_whitespace()   // empty or whitespace-only
vd::string_rules::email_like()            // ^\S+@\S+\.\S+$ via CTRE
vd::string_rules::uri_like()              // ^\w+://\S+$ via CTRE
vd::string_rules::regex(R"(\d{5})")       // runtime std::regex (full-string match)
```

All checkers accept `std::string_view`; `std::string` fields work without explicit conversion.

### Assert module

`vd::require` is a type-safe, source-aware replacement for `assert()`.

```cpp
// Abort on failure (prints file, line, function to stderr)
vd::require(ptr != nullptr, "Expected non-null pointer in {}", __func__);

// Throw on failure
vd::require<std::runtime_error>(value > 0, "Value must be positive, got {}", value);

// Custom callback on failure (zero-overhead NTTP)
void my_logger(std::string msg) { /* … */ }
vd::require_callback<my_logger>(ok, "Validation failed: {}", reason);
```

The format string is checked at compile time via `std::format_string`. Source location is captured at the call site — diagnostics always point to your code, not library internals.

**Output on failure:**
```
Assertion failed: Expected non-null pointer in foo
File: src/foo.cpp
Line: 42
Function: void foo()
```

### Qt extension

Drop-in counterparts for Qt types:

```cpp
#include <vd.hxx>  // Qt extensions included automatically when VD_EXTENSION_QT_BASE=ON

// QString checkers (mirror of vd::string_rules)
vd::qt::string_rules::non_empty()
vd::qt::string_rules::email_like()
vd::qt::string_rules::regex(R"(\+7\d{10})")  // uses QRegularExpression / PCRE2

// Q_PROPERTY validation
auto model = vd::basic_model<MyQObject>()
    .with(vd::qt::qt_property<MyQObject>("email", vd::qt::string_rules::email_like()))
    .with(vd::qt::qt_property<MyQObject>("age",   vd::int_bounds::inclusive(18, 120)));
```

`qt_property` reads property values through the Qt meta-object system at validation time. Returns `false` (without abort) if the property does not exist or the value cannot be converted to the expected type.

The Qt `empty_or_whitespace()` checker uses `QChar::isSpace()` and covers all Unicode whitespace, unlike the std version which checks ASCII only.

## Extending the library

Any callable `V → bool` is a valid checker — no inheritance or registration required:

```cpp
// Lambda
vd::member(&Product::price, [](double v) { return v > 0 && v < 1e6; })

// Stateful checker struct
struct min_length {
    std::size_t n;
    bool operator()(std::string_view s) const noexcept { return s.size() >= n; }
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

## Project structure

```
src/
├── vd.hxx                    # Single public header
├── assert/vd_assert.hxx      # vd::require / vd::require_callback
├── models/
│   ├── vd_rule.hxx           # rule<T>, factory functions
│   ├── vd_basic_model.hxx    # basic_model<T>, basic_bound_model<T>
│   ├── vd_numeric.hxx        # numeric_bounds<T> and type aliases
│   └── vd_string_rules.hxx   # string_match, regex_checker, factories
└── inline_deps/ctre.hpp      # Bundled CTRE (compile-time regex)

ext/qt/
├── vd_qtbase.hxx             # Aggregate Qt Base header
└── qtbase/
    ├── vd_qstring.hxx        # qstring_match, qregex_checker, factories
    └── vd_qproperty.hxx      # qt_property()
```

## License

MIT — see [LICENSE](LICENSE).
