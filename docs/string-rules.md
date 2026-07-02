# String rules module

**Header:** `#include <vd.hxx>`  
**Implementation files:** `src/models/vd_string_rules.hxx` (template part), `src/models/vd_string_rules.cxx` (`regex_checker` and the `regex()` factory, factored out separately since they're non-template and pull in `<regex>`)  
**Namespace:** `vd::string_rules`  
**Dependency:** CTRE (compile-time regular expressions, `src/inline_deps/ctre.hpp`)

---

## Purpose

The module provides checkers for string fields. All checkers accept `std::string_view` and return `vd::result`, so they satisfy the `value_checker` concept (which requires convertibility to `bool`) and work with `vd::member` / `vd::field` for fields of type `std::string`.

```cpp
auto model = vd::basic_model<User>()
    .with(vd::member(&User::name,    vd::string_rules::non_empty()))
    .with(vd::member(&User::email,   vd::string_rules::email_like()))
    .with(vd::member(&User::website, vd::string_rules::uri_like()));
```

---

## `string_match<Matcher>` — the core checker type

```cpp
template<auto Matcher>
    requires string_matcher<Matcher>
struct string_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;
    std::string_view check_description = "string check failed";

    constexpr string_match() = default;
    constexpr string_match(mode m);
    constexpr string_match(mode m, std::string_view description);

    vd::result operator()(std::string_view s) const;
};
```

The `check_description` field is used as the error text in `vd::result::failed_rules` when the rule fails. Factory functions (`empty()`, `non_empty()`, etc.) provide a meaningful description automatically.

### The `Matcher` template parameter

`Matcher` is an **NTTP** (non-type template parameter): a compile-time value, invocable as `Matcher(string_view)`. In practice it's a `bool(std::string_view)` function from the `detail` namespace.

The `string_matcher` concept:
```cpp
template<auto Matcher>
concept string_matcher =
    std::invocable<decltype(Matcher), std::string_view>
    && std::convertible_to<std::invoke_result_t<decltype(Matcher), std::string_view>, bool>;
```

### `mode::include` / `mode::exclude`

`mode::exclude` inverts the matcher's result — this lets you write "not an empty string" without a separate function (though `non_empty()` exists as a dedicated factory for convenience).

```cpp
// Equivalent expressions:
vd::string_rules::non_empty()
// and manually:
vd::string_rules::string_match<vd::string_rules::detail::empty_string>{
    vd::string_rules::string_match<...>::mode::exclude
}
```

---

## Generalization over `CharT`: not just `std::string_view`

`string_match<Matcher>` is still formally templated only on the NTTP `Matcher` — there's no `CharT` in the struct's template parameter list. Generalization over the character type is achieved via **`operator()` overloads** on the same struct (`src/models/vd_string_rules.hxx`):

```cpp
// 1. std::string_view — fixed, always available:
vd::result operator()(std::string_view s) const;

// 2. Any std::basic_string_view<CharT, Traits> with CharT != char
//    (wchar_t, char8_t, char16_t, char32_t) — if Matcher can accept it:
template<typename CharT, typename Traits>
requires(!std::same_as<CharT, char>) && /* Matcher invocable with this view */
vd::result operator()(std::basic_string_view<CharT, Traits> s) const;

// 3. std::basic_string<CharT, Traits, Alloc> with CharT != char — forwards to (2):
template<typename CharT, typename Traits, typename Alloc>
requires(!std::same_as<CharT, char>)
vd::result operator()(const std::basic_string<CharT, Traits, Alloc>& s) const
{
    return (*this)(std::basic_string_view<CharT, Traits>(s));
}
```

**Why three overloads instead of a single `template<typename CharT>`:** `char` is intentionally factored into a separate non-template overload (1) so that it never competes with the template one (2) — otherwise `std::string_view` would match both at once and the compiler couldn't unambiguously pick the best overload. Overload (3) is needed separately because `std::basic_string<CharT>` doesn't deduce to `std::basic_string_view<CharT>` via template argument deduction (there's no deduction guide that would do this automatically in this context) — so the conversion is done explicitly inside the function body.

`Matcher` itself remains a compile-time NTTP function; which character types it can accept depends only on its own signature (see below).

### What's actually generalized and what isn't

| Factory | Works with `wchar_t`/`char8_t`/`char16_t`/`char32_t`? |
|---|---|
| `empty()`, `non_empty()`, `empty_or_whitespace()` | ✅ yes — the corresponding `detail` matchers are themselves templated on `CharT` |
| `min_length()`, `max_length()`, `length_in_between()` | ✅ yes — they count `s.size()` directly, without parsing the content |
| `email_like()`, `uri_like()` | ❌ no — the matchers (`detail::email_like`, `detail::uri_like`) are ordinary `constexpr bool(std::string_view)` built on CTRE, not templates; only overload (1) resolves for them |
| `regex()` | ❌ no — `regex_checker` only accepts `std::string_view`, runtime `std::regex` |

```cpp
vd::string_rules::empty()(std::wstring_view(L""));           // OK
vd::string_rules::non_empty()(std::u16string_view(u"hi"));   // OK
vd::string_rules::non_empty()(std::u32string_view(U"hi"));   // OK
vd::string_rules::empty()(std::wstring(L""));                // OK, via overload (3)

// vd::string_rules::email_like()(std::wstring_view(L"a@b.c"));  // does not compile — no overload accepting wstring_view
```

---

## String length rules: `min_length` / `max_length` / `length_in_between`

```cpp
constexpr detail::min_length_t         min_length(std::size_t min_len);
constexpr detail::max_length_t         max_length(std::size_t max_len);
constexpr detail::length_in_between_t  length_in_between(std::size_t min_len, std::size_t max_len);
```

They check **string length in code units** — the count of `CharT` elements in `basic_string_view<CharT>`/`basic_string<CharT>`, not the count of grapheme clusters (user-perceived characters). For `char` strings this is effectively a byte count; for `char16_t` — UTF-16 units; for `char32_t` — code points. For languages with complex scripts or emoji (combining sequences, surrogate pairs, etc.) this does **not** match "number of characters on screen" — the library doesn't hide this and warns about it in the source doc comments.

```cpp
auto model = vd::basic_model<Profile>()
    .with(vd::field(&Profile::get_email, vd::string_rules::max_length(20)))
    .with(vd::member(&Profile::website,  vd::string_rules::min_length(5)))
    .with(vd::field(&Profile::get_name,  vd::string_rules::length_in_between(3, 10)));
```

They work with the same set of `CharT` as `empty`/`non_empty` — `std::string`, `std::wstring`, `std::u16string`, `std::u32string` and their `_view` counterparts.

### Constructor parameter validation

The `min_length_t`/`max_length_t`/`length_in_between_t` constructors are `constexpr`, but validate their arguments via `vd::ct_require<vd::assertion_exception>` (see [assert.md](assert.md#vdct_require)) — meaning they **throw an exception at runtime** if the parameters are invalid (this is a runtime check over runtime `std::size_t` arguments, not a compile-time error):

```cpp
vd::string_rules::max_length(0);              // throw vd::assertion_exception: "max_len must be positive"
vd::string_rules::min_length(0);               // throw vd::assertion_exception: "min_len must be positive"
vd::string_rules::length_in_between(0, 5);      // throw: "min_len must be positive"
vd::string_rules::length_in_between(10, 5);     // throw: "max_len must be greater than or equal to min_len"
```

---

## Factory functions

All functions return a checker compatible with `value_checker`.

### `empty()`

```cpp
string_match<detail::empty_string> empty();
```

Returns `true` only for an empty string `""`. Whitespace-only strings (`" "`, `"\t"`) are **not** considered empty.

### `non_empty()`

```cpp
string_match<detail::non_empty_string> non_empty();
```

Returns `true` for any non-empty string, including strings made only of spaces.

### `empty_or_whitespace()`

```cpp
string_match<detail::empty_or_whitespace_string> empty_or_whitespace();
```

Returns `true` if the string is empty or consists only of `' '`, `'\t'`, `'\n'`, `'\r'`, `'\f'`, `'\v'` characters.

### `email_like()`

```cpp
string_match<detail::email_like> email_like();
```

A minimal heuristic: the pattern `^\S+@\S+\.\S+$` via CTRE. Checks for the presence of `@`, a domain, and a dot. Full RFC 5322 validation is **not** the intent of this function.

Examples:
```
"user@example.com"    → true
"a@b.c"               → true
"notanemail"          → false
"user@"               → false
"@domain.com"         → false (no local part — \S+ won't match)
```

### `uri_like()`

```cpp
string_match<detail::uri_like> uri_like();
```

A minimal heuristic: the pattern `^\w+://\S+$` via CTRE. Requires a scheme and `://`. Full URI validation is not the intent.

Examples:
```
"https://example.com"  → true
"ftp://files.org/path" → true
"example.com"          → false
"://bad"               → false
```

---

## `regex_checker` — runtime pattern

```cpp
struct regex_checker {
    enum class mode { include, exclude };

    std::regex pattern;
    mode match_mode = mode::include;

    vd::result operator()(std::string_view s) const;
};
```

### The `regex()` factory function

```cpp
regex_checker regex(std::string_view pattern);
```

Creates a checker that validates a string via `std::regex_match` (i.e. the pattern must match the **entire** string, not a substring).

```cpp
auto model = vd::basic_model<Form>()
    .with(vd::member(&Form::postal_code, vd::string_rules::regex(R"(\d{5}(-\d{4})?)")));
```

### Why `regex_checker` is a separate type instead of `string_match<...>`

`string_match<Matcher>` uses the NTTP parameter `auto Matcher`. In C++20, an NTTP can be a function (function pointer) or a struct with `constexpr` fields, but **not a capturing lambda** and not a callable with runtime state.

An `std::regex` pattern is runtime data (a string). So `regex_checker` stores the pattern as an `std::string` field rather than a template parameter. The type remains a fully-fledged `value_checker` via its own `operator()(std::string_view)`.

### Invalid pattern

On an invalid pattern, `std::regex` throws `std::regex_error`. This exception is caught and translated into `vd::require(false, ...)` → `std::abort()` with a diagnostic message on `stderr`.

```cpp
auto bad = vd::string_rules::regex("[invalid");
bad("anything");  // -> abort: "Invalid regex pattern: [invalid. Error code: N"
```

---

## Usage with `std::string` fields

`string_match::operator()` and `regex_checker::operator()` accept `std::string_view`. `std::string` implicitly converts to `std::string_view`, so fields of type `std::string` work without extra conversions:

```cpp
struct User { std::string email; };

// std::invoke(&User::email, obj) returns const std::string&
// The checker accepts std::string_view — implicit conversion
vd::member(&User::email, vd::string_rules::email_like())
```

---

## Writing your own string checker

Any callable `std::string_view -> bool` (or `-> vd::result`) is a valid `value_checker` for string fields:

```cpp
// Via a lambda (returning bool)
auto starts_with_http = [](std::string_view s) {
    return s.starts_with("http");
};
vd::member(&Config::base_url, starts_with_http)

// Via a struct with a detailed error (returning vd::result)
struct min_length {
    std::size_t n;
    vd::result operator()(std::string_view s) const {
        if(s.size() >= n) return vd::result::ok();
        return vd::result::failed({std::format("string must be at least {} chars", n)});
    }
};
vd::member(&Post::body, min_length{10})
```

To use it with `string_match<Matcher>` (with `mode::include/exclude` support), you need a stateless `bool(std::string_view)` function (so it can be an NTTP):

```cpp
namespace my_matchers {
    bool starts_with_http(std::string_view s) { return s.starts_with("http"); }
}

// string_match with exclude mode:
vd::string_rules::string_match<my_matchers::starts_with_http>{
    vd::string_rules::string_match<my_matchers::starts_with_http>::mode::exclude
}
// Checks: does NOT start with "http"
```

---

## CTRE dependency

`email_like()` and `uri_like()` use CTRE for compile-time regex compilation. CTRE is a header-only library, located at `src/inline_deps/ctre.hpp`. The rest of the module's functions don't require CTRE.
