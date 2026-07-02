# Qt extensions module

**Headers:** `#include "ext/qt/vd_qtbase.hxx"` — Qt Base (QString, QProperty)  
**Implementation files:** `src/ext/qt/qtbase/vd_qstring.hxx`, `src/ext/qt/qtbase/vd_qstring.cxx`, `src/ext/qt/qtbase/vd_qproperty.hxx`  
**Namespace:** `vd::qt`, `vd::qt::string_rules`  
**Dependency:** Qt 5/6 (QtCore), CTRE (`src/inline_deps/ctre.hpp`)

---

## Purpose

The Qt extension adds checkers and rule factories that work with Qt types directly — without converting to standard C++ types. All types in this module satisfy the `value_checker` concept and are used with the same `vd::member` / `vd::field` / `vd::predicate` as in the base library.

```cpp
#include "ext/qt/vd_qtbase.hxx"

struct RegistrationForm : QObject {
    Q_OBJECT
    Q_PROPERTY(QString email READ email)
    Q_PROPERTY(int age READ age)
    // ...
};

auto form_model = vd::basic_model<RegistrationForm>()
    .with(vd::qt::qt_property<RegistrationForm>(
              "email", vd::qt::string_rules::email_like()))
    .with(vd::qt::qt_property<RegistrationForm>(
              "age", vd::int_bounds::inclusive(18, 120)));
```

---

## Module structure

```
src/ext/qt/
├── vd_qtbase.hxx          # Aggregating header: qtbase submodules
│   ├── qtbase/
│   │   ├── vd_qstring.hxx # qstring_match, qregex_checker, factories
│   │   └── vd_qproperty.hxx # qt_property()
│
├── vd_qtwidgets.hxx       # Reserved (currently empty)
└── vd_qml.hxx             # Reserved (currently empty)
```

---

## `vd::qt::string_rules` — checkers for QString

A mirror of `vd::string_rules`, rewritten for `QStringView` / `QString`. The API is identical, the behavior adapted to Qt semantics.

### `qstring_match<Matcher>` — the core checker type

```cpp
template<auto Matcher>
    requires qstring_matcher<Matcher>
struct qstring_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;
    std::string_view check_description = "string check failed";

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m);
    constexpr qstring_match(mode m, std::string_view description);

    vd::result operator()(QStringView s) const;
};
```

The `check_description` field is passed to `vd::result::failed_rules` on failure. Factory functions set a meaningful description automatically.

An analogue of `vd::string_rules::string_match<Matcher>`, but operating on `QStringView` instead of `std::string_view`. Since `const QString&` implicitly converts to `QStringView`, the checker is compatible with `vd::member` on fields of type `QString`:

```cpp
struct Profile {
    QString username;
    QString bio;
};

auto model = vd::basic_model<Profile>()
    .with(vd::member(&Profile::username, vd::qt::string_rules::non_empty()))
    .with(vd::member(&Profile::bio,      vd::qt::string_rules::empty_or_whitespace()));
```

### The `qstring_matcher` concept

```cpp
template<auto Matcher>
concept qstring_matcher =
    std::invocable<decltype(Matcher), QStringView>
    && std::convertible_to<std::invoke_result_t<decltype(Matcher), QStringView>, bool>;
```

The NTTP parameter `Matcher` is a stateless `bool(QStringView)` function. The requirements are the same as for `string_matcher` in the base module.

### `mode::include` / `mode::exclude`

`mode::exclude` inverts the matcher's result — the same mechanism as in the base `string_match`. Constructing with `mode` directly is rarely needed, since factories cover all the standard cases.

---

## Factory functions

All functions return a checker compatible with `value_checker`.

### `empty()`

```cpp
qstring_match<detail::empty_string> empty();
```

`true` only for `QString("")`. Uses `QStringView::isEmpty()`.

### `non_empty()`

```cpp
qstring_match<detail::non_empty_string> non_empty();
```

`true` for any non-empty string, including strings made only of spaces.

### `empty_or_whitespace()`

```cpp
qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace();
```

`true` if the string is empty or consists only of characters for which `QChar::isSpace()` returns `true`.

**Difference from the std version:** the base `empty_or_whitespace_string` checks a limited set of ASCII whitespace (`' '`, `'\t'`, `'\n'`, `'\r'`, `'\f'`, `'\v'`). The Qt version uses `QChar::isSpace()`, which covers all Unicode whitespace (U+00A0 NO-BREAK SPACE, U+2003 EM SPACE, etc.), matching Qt idioms for working with internationalized text.

### `email_like()`

```cpp
qstring_match<detail::email_like> email_like();
```

A minimal heuristic: the pattern `^\S+@\S+\.\S+$` via CTRE. The string is converted to UTF-8 (`QStringView::toUtf8()`) before being passed to CTRE — the same compile-time patterns as in the base module.

### `uri_like()`

```cpp
qstring_match<detail::uri_like> uri_like();
```

The pattern `^\w+://\S+$` via CTRE. Same UTF-8 conversion logic.

---

## Length rules: `min_length` / `max_length` / `length_in_between`

A mirror of the std version from [string-rules.md](string-rules.md#string-length-rules-min_length--max_length--length_in_between), but without `CharT` generalization — `QString` is always UTF-16 internally, so a separate template over the character type isn't needed:

```cpp
constexpr detail::min_length_t         min_length(std::size_t min_len);
constexpr detail::max_length_t         max_length(std::size_t max_len);
constexpr detail::length_in_between_t  length_in_between(std::size_t min_len, std::size_t max_len);
```

Each checker calls `QStringView::size()` and compares it against the bounds:

```cpp
struct max_length_t final {
    std::size_t max_len;
    constexpr max_length_t(std::size_t max_len);   // throws vd::assertion_exception if max_len == 0

    vd::result operator()(QStringView s) const;    // error if s.size() > max_len
};
```

```cpp
auto model = vd::basic_model<Profile>()
    .with(vd::member(&Profile::username, vd::qt::string_rules::min_length(3)))
    .with(vd::member(&Profile::bio,      vd::qt::string_rules::max_length(280)));
```

### Unit of measurement: UTF-16 code units, not bytes and not graphemes

`QStringView::size()` is the count of **UTF-16 code units** (2-byte machine words of `QString`'s internal UTF-16 representation). This **does not match** what the std version counts for `char` strings (there — bytes/UTF-8 code units): a length limit given as a number for `vd::string_rules::max_length` does not carry over literally to `vd::qt::string_rules::max_length` for the same text — the units differ. As with the std version, surrogate pairs and composite grapheme clusters (emoji, combining diacritics) are counted differently from "characters on screen".

### Constructor parameter validation

As with the std version, the constructors throw `vd::assertion_exception` via `vd::ct_require` on invalid arguments (`max_len == 0`, `min_len == 0`, `max_len < min_len`) — this is a runtime check, not a compile error, despite `constexpr`:

```cpp
vd::qt::string_rules::max_length(0);              // throw vd::assertion_exception
vd::qt::string_rules::length_in_between(10, 5);   // throw vd::assertion_exception
```

---

## `qregex_checker` — runtime pattern via QRegularExpression

```cpp
struct qregex_checker {
    enum class mode { include, exclude };
    QString pattern;
    mode match_mode = mode::include;

    vd::result operator()(QStringView s) const;
};
```

An analogue of `vd::string_rules::regex_checker`, but using `QRegularExpression` (PCRE2) instead of `std::regex`.

### The `regex()` factory function

```cpp
qregex_checker regex(QString pattern);
```

```cpp
auto model = vd::basic_model<Form>()
    .with(vd::member(&Form::phone,
                     vd::qt::string_rules::regex(R"(\+7\d{10})")));
```

### Match semantics

A **full-string match** is used (analogous to `std::regex_match`, not `std::regex_search`): a match counts only if `capturedStart() == 0` and `capturedLength() == s.size()`. The pattern must describe the entire string.

### Invalid pattern

If `QRegularExpression::isValid()` returns `false`, `vd::require(false, ...)` is called → `std::abort()` with diagnostics on `stderr`.

```cpp
auto bad = vd::qt::string_rules::regex("[invalid");
bad(QStringView{});  // abort: "Invalid regex pattern: [invalid"
```

### Advantage over `std::regex`

`QRegularExpression` is based on PCRE2 and supports full Unicode out of the box, named groups, look-ahead/behind, and other PCRE2 features unavailable in `std::regex`.

---

## `vd::qt::qt_property` — validating Q_PROPERTY

```cpp
template<typename T, typename Checker>
auto qt_property(const QString& prop_name, Checker checker) -> rule<T>;
```

Creates a `rule<T>` that reads a Qt property (declared via `Q_PROPERTY`) and validates its value with a checker. `T` must derive from `QObject`.

```cpp
struct Widget : QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(bool enabled READ isEnabled)
};

auto model = vd::basic_model<Widget>()
    .with(vd::qt::qt_property<Widget>(
              "text", vd::qt::string_rules::non_empty()))
    .with(vd::qt::qt_property<Widget>(
              "enabled", [](bool v) { return v; }));
```

### Property type deduction

The property's value type `PropT` is deduced from `Checker`'s first argument via `detail::first_arg_of<Checker>`. This is the same trait used in the base `vd::predicate`.

**Limitation:** type deduction is impossible for generic lambdas and `std::function`. In such cases, create `rule<T>` explicitly:

```cpp
vd::rule<Widget> r([](const Widget& w) {
    return w.property("count").toInt() > 0;
});
```

### Chain of checks inside the rule

The rule returns a `vd::result` with an error in the following cases (no abort, just a failing rule):

| Situation | Behavior |
|----------|-----------|
| `T` is not a `QObject` | `vd::result::failed({"Object is not a QObject"})` |
| Property `prop_name` doesn't exist | `vd::result::failed({"Property is not valid"})` |
| The `QVariant` value can't be converted to `PropT` | `vd::result::failed({"Property cannot be converted to expected type"})` |
| The checker returns a failure | `vd::result::failed({...message from the checker...})` |

The property name is stored as `QByteArray` (UTF-8) inside the closure — the conversion from `QString` happens once when the rule is created, not on every check.

---

## Inclusion

The module is included via `<vd.hxx>` **automatically** when the CMake option `VD_EXTENSION_QT_BASE` is set to `ON`. In that case, the compiler gets the `VD_ENABLE_EXTENSION_QT_BASE` macro, and `vd_ext.hxx` pulls in `vd_qtbase.hxx` as part of the main header.

```cmake
set(VD_EXTENSION_QT_BASE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Validate)
target_link_libraries(my_target PRIVATE Validate::vd Qt6::Core)
```

```cpp
#include <vd.hxx>  // Qt extensions are already included
```

If needed, submodules can be included explicitly (e.g. in projects without FetchContent):

```cpp
// The whole qtbase (QString + QProperty):
#include "ext/qt/vd_qtbase.hxx"

// Only the QString checkers:
#include "ext/qt/qtbase/vd_qstring.hxx"

// Only qt_property:
#include "ext/qt/qtbase/vd_qproperty.hxx"
```

---

## Planned submodules

| File | Status | Purpose |
|------|--------|------------|
| `vd_qtwidgets.hxx` | Reserved | Checkers for QWidget properties |
| `vd_qml.hxx` | Reserved | Integration with the QML context |
