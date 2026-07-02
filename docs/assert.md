# Assert module

**Header:** `#include <vd.hxx>` (or `#include "assert/vd_assert.hxx"` directly)  
**Namespace:** `vd`

## Purpose

The assert module is a set of tools for checking preconditions. It's used both inside the implementation (`vd::require` in `basic_bound_model`) and in checkers (`vd::string_rules::detail::std_regex`). It can also be used in user code.

Key property: on a failed condition, the error message automatically includes the **file, line, and function name** — the call site of `require`, not the library internals. This is achieved by capturing `std::source_location::current()` in the format-string parameter via a `consteval` constructor.

## API

### `concept contextually_bool`
A helper concept for `vd::require` templates, describing the requirement that an arbitrary object of type `T` support *contextual bool conversion* — i.e. it can be used inside conditions (`if` and the ternary operator) without an explicit conversion to `bool` via `static_cast<>` or other mechanisms.

### `vd::require`

```cpp
template<detail::contextually_bool Cond, typename... Args>
void vd::require(Cond&& condition, format_string fmt, Args&&... args);
```

The `condition` parameter can be any type allowing contextual conversion to `bool`.

If `condition == false` — formats the message via `std::format`, prints it to `stderr`, and calls `std::abort()`.

```cpp
vd::require(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require(value > 0, "Value must be positive, got {}", value);
vd::require(true, "This never fires");
```

The format string is compile-time: argument types are checked at compile time via `std::format_string<Args...>`.

**Output on failure:**
```
Assertion failed: Expected non-null pointer in foo
File: src/foo.cpp
Line: 42
Function: void foo()
```

### `vd::require<exception>`
```cpp
template<detail::contextually_bool Cond, typename ExceptionType, typename... Args>
    requires std::derived_from<ExceptionType, std::exception>
void vd::require(Cond&& condition, format_string fmt, Args&&... args);
```

If `condition == false` — formats the message via `std::format` and throws `ExceptionType`, constructing it from the formatted string (passed to the constructor as `std::string`). This means the exception's `what()` will contain exactly the message text — without the file, line, or function (unlike the abort version).

`ExceptionType` must derive from `std::exception` and accept a `std::string` in its constructor.

```cpp
vd::require<std::logic_error>(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require<vd::validation_exception>(value > 0, "Value must be positive, got {}", value);
vd::require<std::runtime_error>(true, "This never fires");
```

**Output on failure:**
```
// exception.what() == "Expected non-null pointer in foo"
```

No output to `stderr`. The call site (`source_location`) is captured but not used in the exception text.

### `vd::require_callback`

```cpp
template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
    requires std::invocable<decltype(OnFailed), std::string_view>
void vd::require_callback(Cond&& condition, format_string fmt, Args&&... args);
```

Instead of `abort()`, calls the given NTTP callable with the formatted message. Useful for testing and for hooking into a logging system.

```cpp
void my_logger(std::string msg) { std::cerr << "[ERROR] " << msg << "\n"; }

vd::require_callback<my_logger>(value > 0, "Bad value: {}", value);
```

`OnFailed` is a template non-type parameter, so the callback is resolved at compile time with no `std::function` overhead. The callback receives a `std::string_view` into the formatted message.

### `vd::ct_require`

```cpp
template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
    requires std::derived_from<ExceptionType, std::exception>
constexpr void ct_require(Cond&& condition, format_string fmt, Args&&... args);
```

A compile-time-compatible variant of `require<ExceptionType>`: marked `constexpr`, which allows it to be used inside `constexpr` constructors (e.g. in the constructors of `vd::string_rules::detail::min_length_t`/`max_length_t`/`length_in_between_t`, see [string-rules.md](string-rules.md)). On a failed condition — just like `require<ExceptionType>` — it throws `ExceptionType`, constructed from the formatted string.

Important: the `constexpr` marker does **not turn the check into a compile-time error** — if the arguments (`condition`, format parameters) are not `constexpr` expressions (e.g. an ordinary runtime `std::size_t` passed to the constructor), the check behaves as an ordinary runtime check that throws an exception. `ct_require` simply doesn't prevent itself from being used in contexts that require `constexpr`-compatible signatures (unlike `require`, which doesn't have that compatibility).

```cpp
struct max_length_t final {
    std::size_t max_len;
    constexpr max_length_t(std::size_t max_len) : max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
    }
    // ...
};

vd::string_rules::max_length(0);   // throw vd::assertion_exception: "max_len must be positive"
```

Unlike `require<ExceptionType>`, `ct_require` has no variant without `ExceptionType` (no `abort()` overload) and no `require_callback` counterpart — throw semantics only.

### `vd::assertion_exception`

```cpp
// src/core/vd_exception.hxx
using assertion_exception = vd_tagged_exception<struct assertion_exception_tag>;
```

A ready-made exception tag class (`std::exception`, `what()` returns the formatted message), intended specifically for argument/precondition check failures via `vd::ct_require` — kept separate from `vd::validation_exception` (which is semantically tied to *data validation* failures through `basic_model`/`static_model`). Both are specializations of the same `vd_tagged_exception<Tag>` template with different tags, so they don't mix with each other in `catch` blocks, even though internally they're structured identically.

---

## Internals

### `assert_format<Args...>`

A helper struct that holds both a `std::format_string<Args...>` and a `std::source_location`. Its constructor is `consteval`, which lets `std::source_location::current()` capture the call site of `require`, rather than the definition site of `assert_format` itself.

The `std::type_identity_t<Args>...` trick in `require`'s signature exists so that `Args` deduction comes from the trailing arguments rather than from the format string (otherwise the compiler can't resolve two independent deductions for the same `Args`):

```cpp
// require's signature:
void require(Cond&& condition,
             details::assert_format<std::type_identity_t<Args>...> fmt_loc,
             Args&&... args);
//                    ^^^^^^^^^^^^^^^^ <- Args is deduced from here
//                                                          ^^^^ <- not from here
```

### `assert_fail`

A `[[noreturn]]` function that formats and prints the message to `stderr`, then calls `std::abort()`. Factored out separately to reduce code size when `require` is instantiated with different sets of `Args`.

### Debug overloads

Each of `require`, `require<>`, `require_callback` has Debug overloads named `required`, `required<>`, `require_callbackd`, which only work in builds without the `_NDEBUG` symbol defined (with an underscore, not `NDEBUG`).

```cpp
#ifndef _NDEBUG
// debug-only implementations
void required(Cond&& condition, format_string fmt, Args&&... args);
template<typename ExceptionType, ...> void required(...);
template<auto OnFailed, ...> void require_callbackd(...);
#else
// empty no-op stubs
#endif
```

They don't differ in mechanics from the regular functions inside debug builds, but are removed in release. In the release version, `require_callbackd` has the constraint `std::invocable<decltype(OnFailed), std::string>` (instead of `std::string_view` as in debug) — this is a known inconsistency in the code; the stub doesn't call anything anyway.
