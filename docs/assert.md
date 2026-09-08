# Assert module

**Header:** `#include <vd.hxx>` (or `#include "assert/vd_assert.hxx"` directly)  
**Namespace:** `vd`

## Purpose

The assert module is a set of tools for checking preconditions. It's used both inside the implementation (`vd::require<vd::validation_exception>` in `basic_model::die_if_failed` / `static_model::die_if_failed`, `vd::ct_require` in the length-rule constructors) and in checkers (`vd::string_rules::regex`). It can also be used in user code.

Key property: on a failed condition, the error message automatically includes the **file, line, and function name** — the call site of the check, not the library internals. This is achieved by capturing `std::source_location::current()` in the format-string parameter via a `consteval` constructor.

This decoration is applied **uniformly by every flavour** of the check: the text carried by a thrown exception, the text printed to `stderr`, and the text handed to a callback are all produced by the same `vd::detail::format_fail`, so they can never drift apart.

```
Assertion failed: Expected non-null pointer in foo
File: src/foo.cxx
Line: 42
Function: void foo()
```

## Choosing a flavour

| Function | On failure | Available in release |
|---|---|---|
| `vd::strict_require` | prints to `stderr`, then `std::abort()` | yes |
| `vd::require` | throws `vd::assertion_exception` | yes |
| `vd::require<E>` | throws `E` | yes |
| `vd::require_cb<F>` | calls `F` with the message | yes |
| `vd::ct_require<E>` | throws `E`; usable in `constexpr` contexts | yes |
| `vd::strict_required` | as `strict_require` | **no — compiled out** |
| `vd::required` | as `require` | **no — compiled out** |
| `vd::required<E>` | as `require<E>` | **no — compiled out** |
| `vd::require_cbd<F>` | as `require_cb<F>` | **no — compiled out** |

## API

### `concept contextually_bool`

A helper concept for the `require` family, describing the requirement that an object of type `T` support *contextual bool conversion* — i.e. it can be used inside conditions (`if`, the ternary operator) without an explicit `static_cast<bool>`.

```cpp
template<typename T>
concept vd_contextually_bool_impl = requires(const T& t) { static_cast<bool>(t); };

template<typename T>
concept contextually_bool = vd_contextually_bool_impl<T> && requires(const T& t) {
    { !t } -> vd_contextually_bool_impl;
};
```

Both requirements are checked on a **`const` lvalue**, deliberately: the whole family takes its condition as `const Cond&`, so a type whose `operator bool()` or `operator!()` is not `const`-qualified must be rejected by the constraint at the call site rather than fail inside the template body. The constness of the parameter and the constness in the concept are one decision, not two — changing either alone silently breaks the diagnostics.

The second conjunct exists because the implementation writes `if(!condition)`, not `if(!static_cast<bool>(condition))`: a type may provide `operator bool` while its `operator!` returns something unusable, and that has to be caught too.

Satisfied by `bool`, integers, raw pointers, `std::optional`, `std::unique_ptr`, `std::shared_ptr`, `vd::result`, and any type with a `const`-qualified explicit `operator bool`. Not satisfied by `std::string`.

### `vd::strict_require`

```cpp
template<detail::contextually_bool Cond, typename... Args>
void strict_require(const Cond& condition, format_string fmt, Args&&... args);
```

If `condition` is falsy — formats the message, prints the decorated text to `stderr`, and calls `std::abort()`. This is the unrecoverable flavour: use it for invariants whose violation means the process has no meaningful way to continue.

```cpp
vd::strict_require(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::strict_require(value > 0, "Value must be positive, got {}", value);
```

The format string is compile-time: argument types are checked at compile time via `std::format_string<Args...>`.

### `vd::require`

```cpp
template<detail::contextually_bool Cond, typename... Args>
void require(const Cond& condition, format_string fmt, Args&&... args);
```

If `condition` is falsy — throws `vd::assertion_exception` whose `what()` is the decorated message (body **plus** file, line and function).

```cpp
vd::require(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require(value > 0, "Value must be positive, got {}", value);
```

Nothing is written to `stderr` — the call site travels inside the exception instead.

### `vd::require<ExceptionType>`

```cpp
template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
    requires std::derived_from<ExceptionType, std::exception>
void require(const Cond& condition, format_string fmt, Args&&... args);
```

Same as above, but throws `ExceptionType`, constructed from the decorated message (`std::string`). `ExceptionType` must derive from `std::exception` and accept a `std::string` in its constructor.

`ExceptionType` is the **first** template parameter, so it is the only one written explicitly; `Cond` and `Args` stay deduced.

```cpp
vd::require<std::logic_error>(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require<vd::validation_exception>(value > 0, "Value must be positive, got {}", value);
```

### `vd::require_cb`

```cpp
template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
    requires std::invocable<decltype(OnFailed), std::string_view>
void require_cb(const Cond& condition, format_string fmt, Args&&... args);
```

Instead of aborting or throwing, calls the given NTTP callable with the decorated message. Useful for collecting diagnostics and for hooking into a logging system.

```cpp
void my_logger(std::string_view msg) { std::cerr << "[ERROR] " << msg << "\n"; }

vd::require_cb<my_logger>(value > 0, "Bad value: {}", value);
```

`OnFailed` is a template non-type parameter, so the callback is resolved at compile time with no `std::function` overhead. A captureless lambda works too, decayed to a function pointer by the unary `+`:

```cpp
vd::require_cb<+[](std::string_view msg) { collected.emplace_back(msg); }>(value > 0, "Bad value: {}", value);
```

Control returns to the caller afterwards — `require_cb` does not stop execution by itself.

### `vd::ct_require`

```cpp
template<typename ExceptionType, detail::contextually_bool Cond, typename... Args>
    requires std::derived_from<ExceptionType, std::exception>
constexpr void ct_require(const Cond& condition, format_string fmt, Args&&... args);
```

A `constexpr`-compatible variant of `require<ExceptionType>`, which allows it to be used inside `constexpr` constructors (e.g. in the constructors of `vd::string_rules::detail::min_length_t`/`max_length_t`/`length_in_between_t`, see [string-rules.md](string-rules.md)). On a failed condition it throws `ExceptionType`, constructed from the decorated message.

```cpp
struct max_length_t final {
    std::size_t max_len;
    constexpr max_length_t(std::size_t max_len) : max_len(max_len)
    {
        vd::ct_require<vd::assertion_exception>(max_len > 0, "max_len must be positive");
    }
    // ...
};

vd::string_rules::max_length(0);   // throws vd::assertion_exception
```

Important: the `constexpr` marker does **not turn the check into a compile-time error** on its own. In a runtime call it is an ordinary runtime check that throws. What it does give you is that a failing check *in a constant-evaluated context* makes the whole call a non-constant expression — because `throw` is not allowed during constant evaluation — so the compiler rejects the enclosing `constexpr` variable or template argument:

```cpp
constexpr int checked_div(int a, int b)
{
    vd::ct_require<vd::assertion_exception>(b != 0, "division by zero");
    return a / b;
}

constexpr int ok = checked_div(10, 2);   // fine
constexpr int bad = checked_div(10, 0);  // compile error: not a constant expression
```

Unlike `require`, `ct_require` has no overload without `ExceptionType` and no callback counterpart — throw semantics only.

### Debug-only family

`strict_required`, `required`, `required<ExceptionType>` and `require_cbd` mirror `strict_require`, `require`, `require<ExceptionType>` and `require_cb` exactly, with one difference: they exist only in debug builds. In a release build the same names resolve to empty no-op overloads, and the condition is not even evaluated.

```cpp
#if not defined(_NDEBUG) and not defined(NDEBUG) and not defined(RELEASE)
// debug-only implementations
#else
// empty no-op stubs
#endif
```

The guard is a **conjunction of negations**: the family is active only when *none* of the release markers is defined. This matters because MSVC defines `NDEBUG` alone in a release configuration — a disjunction here would keep these checks compiled into every ordinary release build.

The no-op stubs carry the same signatures and the same constraints as the debug implementations, including `const Cond&` and `contextually_bool`. That is deliberate: with a by-value parameter or a looser constraint, a move-only condition would compile in debug and break in release.

Since the condition expression is not evaluated in release, it must not carry side effects you depend on:

```cpp
vd::required(++counter > 0, "...");  // wrong: counter is never incremented in release
```

### `vd::assertion_exception`

```cpp
// src/core/vd_exception.hxx
using assertion_exception = vd_tagged_exception<struct assertion_exception_tag>;
```

A ready-made exception class (`std::exception`, `what()` returns the decorated message), used for argument/precondition check failures — kept separate from `vd::validation_exception` (which is semantically tied to *data validation* failures through `basic_model`/`static_model`). Both are specializations of the same `vd_tagged_exception<Tag>` template with different tags, so they never cross-catch, even though internally they're structured identically.

---

## Source location helpers

**Header:** `#include "utils/vd_sourceloc.hxx"`

This header is **not** pulled in by `<vd.hxx>` or by any of the `vd_*.hxx` umbrella headers — include it directly if you want either of the two things below.

### `vd::here()`

```cpp
consteval std::source_location here(std::source_location real_location = std::source_location::current()) noexcept;
```

A shorthand for `std::source_location::current()`, for when repeating the full name is noise. Being `consteval` with the location as a defaulted argument, it captures the *call site* of `here()` — the same mechanism `assert_format` uses.

You do **not** need it for the `require` family: every flavour already captures the call site on its own and decorates the message with it. `here()` is for your own diagnostics, logging and error types:

```cpp
my_logger.warn("cache miss at {}", vd::here());   // needs the formatter below
throw my_error("bad state", vd::here());          // does not
```

### `VD_BUNDLE_SOURCELOCATION_FORMATTER`

`std::source_location` has no `std::formatter` in the standard library, so `std::format("{}", loc)` does not compile out of the box. Defining `VD_BUNDLE_SOURCELOCATION_FORMATTER` before including the header enables a minimal bundled one:

```cpp
// with the macro defined:
std::format("{}", vd::here());        // "src/foo.cxx:42:19"  -- file:line:column
std::format("[{:>40}]", vd::here());  // parse() is inherited from formatter<string_view>,
                                      // so alignment/width/precision specs work
```

Deliberately minimal: `char` only (no `wchar_t` specialization) and no function name in the output. If you need more, don't define the macro and write your own.

**Why it is opt-in rather than always on.** A program may contain exactly one definition of `std::formatter<std::source_location>`. If your code — or any other library you link — already provides one, an unconditional specialization here would be a redefinition. Making it opt-in leaves that program-wide decision to the consumer, which is the only place it can correctly be made.

**Define it project-wide, not per translation unit.** The macro decides whether a `std::formatter` specialization exists, so defining it in some TUs but not others means the same `std::source_location` formats differently — or fails to format — depending on the TU. For an inline function or a template in one of your own headers, that is an ODR violation, and a quiet one: the linker picks one instantiation and the mismatch never gets diagnosed. Put it on the target instead:

```cmake
target_compile_definitions(my_app PRIVATE VD_BUNDLE_SOURCELOCATION_FORMATTER)
```

The guard is `#if defined(VD_BUNDLE_SOURCELOCATION_FORMATTER)` first, `#include <version>`, and only then `#if defined(__cpp_lib_format)` — in that order on purpose. `<source_location>` does not have to define `__cpp_lib_format`: MSVC happens to, because it defines every `__cpp_lib_*` macro in `yvals_core.h`, but libstdc++ and libc++ define it in `<format>` and `<version>` only. Testing the feature macro before including `<version>` would therefore drop the formatter silently on those implementations — the worst failure mode an opt-in can have.

---

## Internals

### `assert_format<Args...>`

A helper struct that holds both a `std::format_string<Args...>` and a `std::source_location`. Its constructor is `consteval`, which lets `std::source_location::current()` capture the call site of the check, rather than the definition site of `assert_format` itself.

The `std::type_identity_t<Args>...` trick in the signatures exists so that `Args` deduction comes from the trailing arguments rather than from the format string (otherwise the compiler can't resolve two independent deductions for the same `Args`):

```cpp
// require's signature:
void require(const Cond& condition,
             detail::assert_format<std::type_identity_t<Args>...> fmt_loc,
             Args&&... args);
//                     ^^^^^^^^^^^^^^^^ <- Args is deduced from here
//                                                           ^^^^ <- not from here
```

### `format_fail`

```cpp
[[nodiscard]] std::string format_fail(std::string_view message, const std::source_location& loc);
```

Decorates a formatted message body with the call site. Every flavour routes through it, which is what keeps the exception text, the `stderr` text and the callback text identical. Lives in `vd_assert.cxx` so the decoration format is defined in exactly one place.

### `assert_fail`

```cpp
[[noreturn]] void assert_fail(std::string_view message, const std::source_location& loc);
```

The abort path behind `strict_require`/`strict_required`: builds the text via `format_fail`, writes it to `stderr`, then calls `std::abort()`. Factored out into `vd_assert.cxx` to reduce code size when the templates are instantiated with different sets of `Args`.

Printing goes through `std::println` where `__cpp_lib_print` is available (C++23), and falls back to `std::fputs` on the already-built `std::string` otherwise. `fputs` rather than `fprintf("%s", ...)` because at that point the text is a `std::string`, not a format template — and because `std::string_view` carries no NUL-termination guarantee, so it cannot be fed to `%s` directly.

### Passing the condition

The whole family takes `const Cond&`. The alternatives were rejected for concrete reasons: by-value copies heavy conditions and rejects move-only lvalues such as `std::unique_ptr`, while `Cond&&` is a forwarding reference that is never actually forwarded — the condition is only tested, never consumed — which static analysers correctly flag. `const Cond&` copies nothing and accepts everything the concept admits; the price is that the condition's `operator bool`/`operator!` must be `const`-qualified, which is exactly what the concept enforces.
