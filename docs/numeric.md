# Numeric module

**Header:** `#include <vd.hxx>`  
**Implementation file:** `src/models/vd_numeric.hxx`  
**Namespace:** `vd`

---

## Purpose

The module provides two checkers for numeric types, both implementing the `value_checker` concept and both intended as the second argument to `vd::field` / `vd::member`:

- `numeric_bounds<T>` — range checks (inclusive/exclusive, one-sided, outside-range);
- `finite_guard` (as the ready-made object `vd::numeric::finite_t`) — rejects `NaN` and `inf`.

---

## `vd::numeric_bounds<T>`

```cpp
template<numeric_compatible T>
struct numeric_bounds final {
    const T min;
    const T max;

    vd::result operator()(const T& value) const;

    static constexpr numeric_bounds<T> inclusive(T min, T max);
    static constexpr numeric_bounds<T> exclusive(T min, T max);
    static constexpr numeric_bounds<T> greater_than(T min);
    static constexpr numeric_bounds<T> less_than(T max);
    static constexpr numeric_bounds<T> unbounded();
    static constexpr numeric_bounds<T> outside_inclusive(T lower_bound, T upper_bound);
    static constexpr numeric_bounds<T> outside_exclusive(T lower_bound, T upper_bound);
};
```

### The `numeric_compatible` concept

```cpp
// Defined in src/utils/vd_ctnextafter.hxx:
template<typename T>
concept generic_numer = std::integral<T> || std::floating_point<T>;

// Defined in src/models/vd_numeric.hxx:
template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept numeric_compatible = generic_numer<T> && arithmetic<T>;
```

Accepts: `int`, `double`, `float`, `uint8_t`, `int64_t`, etc.  
Rejects: `bool`, classes, pointers.

### Factory methods

| Method | Semantics | Example |
|-------|-----------|--------|
| `inclusive(min, max)` | `value >= min && value <= max` | `[0, 100]` |
| `exclusive(min, max)` | `value > min && value < max` | `(0, 100)` |
| `greater_than(min)` | `value > min` | `(5, +∞)` |
| `less_than(max)` | `value < max` | `(-∞, 10)` |
| `unbounded()` | always `true` | `(-∞, +∞)` |
| `outside_inclusive(lo, hi)` | `value <= lo \|\| value >= hi` | outside `[lo, hi]` |
| `outside_exclusive(lo, hi)` | `value < lo \|\| value > hi` | outside `(lo, hi)` |

### Implementation of exclusive bounds

`exclusive`, `greater_than`, `less_than`, `outside_exclusive` are implemented via the library's `vd::ct_nextafter<T>` — a `constexpr` analogue of `std::nextafter`, supporting both integers and floating-point numbers:

```cpp
// exclusive(0.0, 1.0) creates bounds with:
min = ct_nextafter(0.0, std::numeric_limits<double>::max());    // 0.0 + epsilon
max = ct_nextafter(1.0, std::numeric_limits<double>::lowest()); // 1.0 - epsilon

// exclusive(0, 10) for int:
min = ct_nextafter(0, std::numeric_limits<int>::max());  // 1
max = ct_nextafter(10, std::numeric_limits<int>::min()); // 9
```

This works correctly both for `float`/`double` and for integer types.

---

## Usage examples

```cpp
// int field in range [1, 100]
vd::member(&User::age, vd::int_bounds::inclusive(1, 100))

// double getter strictly greater than zero
vd::field(&Sensor::get_value, vd::double_bounds::greater_than(0.0))

// float strictly less than 1.0 (exclusive)
vd::member(&Data::ratio, vd::float_bounds::exclusive(0.0f, 1.0f))
```

---

## Type aliases

### Bounds

```cpp
using byte_bounds          = numeric_bounds<std::uint8_t>;
using short_bounds         = numeric_bounds<std::int16_t>;
using int_bounds           = numeric_bounds<std::int32_t>;
using long_bounds          = numeric_bounds<std::int64_t>;
using float_bounds         = numeric_bounds<float>;
using double_bounds        = numeric_bounds<double>;

using signed_byte_bounds   = numeric_bounds<std::int8_t>;
using unsigned_short_bounds = numeric_bounds<std::uint16_t>;
using unsigned_int_bounds  = numeric_bounds<std::uint32_t>;
using unsigned_long_bounds = numeric_bounds<std::uint64_t>;
```

### Models

Ready-made `basic_model<T>` aliases for numeric types:

```cpp
using int_model    = basic_model<std::int32_t>;
using double_model = basic_model<double>;
// ... etc.
```

Usage example:
```cpp
vd::int_model age_model;
age_model.with(vd::predicate([](const int& v) { return v > 0 && v < 150; }));
```

---

## `vd::numeric::finite_guard` / `vd::numeric::finite_t`

```cpp
namespace vd::numeric {
    struct finite_guard final {
        template<numeric_compatible T>
        vd::result operator()(const T& value) const;
    };

    inline constexpr auto finite_t = finite_guard {};
}
```

A checker that verifies `std::isfinite(value)` — used to keep `NaN` and `inf` out of floating-point fields. Note that the API is an *object*, not a factory: there is no `finite<T>()` call, you pass `vd::numeric::finite_t` directly.

```cpp
auto model = vd::basic_model<Measurement>()
    .with(vd::member(&Measurement::value, vd::numeric::finite_t))
    .with(vd::member(&Measurement::value, vd::double_bounds::greater_than(0.0)));
```

For an integral `T` the check is always `true` — `std::isfinite` on an integer promotes to `double` and can never yield `inf`/`NaN`. Keeping integral types accepted rather than rejected means the guard can be applied uniformly to a struct whose members are a mix of `double`, `float` and `int` without any per-member special-casing.

### Why the class is not a template

`finite_guard` templates its `operator()`, not itself. Three consequences follow, and all three are the reason for the design:

**A single object serves every arithmetic type.** `finite_t` is one `inline constexpr` instance shared across the whole program, and the same object can be handed to a `double` member, a `float` member and an `int32_t` member of the same model — each rule instantiates `operator()` at its own member type:

```cpp
struct Reading {
    double celsius;
    float humidity;
    std::int32_t samples;
};

auto model = vd::make_static_model<Reading>()
    .with(vd::statics::member("celsius",  &Reading::celsius,  vd::numeric::finite_t))
    .with(vd::statics::member("humidity", &Reading::humidity, vd::numeric::finite_t))
    .with(vd::statics::member("samples",  &Reading::samples,  vd::numeric::finite_t));
```

**The `numeric_compatible` constraint became SFINAE-friendly.** Because the constraint sits on `operator()` rather than on the class, an unsuitable `T` makes the *concept* fail instead of hard-erroring inside an instantiation:

```cpp
static_assert( vd::value_checker<vd::numeric::finite_guard, double>);
static_assert(!vd::value_checker<vd::numeric::finite_guard, std::string>);
```

That means the guard can participate in overload resolution and in `requires`-clauses without taking the program down.

**The guard is empty and trivially copyable**, so `static_model` stores it inline in its rule tuple with no indirection at all, and the resulting model is constexpr-constructible:

```cpp
constexpr auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
static_assert(sizeof(model) == 1);   // the rule occupies no storage
```

Only `check()` runs at runtime: `vd::result` owns a `std::vector<std::string>`, so it cannot be constant-evaluated even when the model itself is a constant.

### As a top-level rule

With `static_model` the guard is passed straight to `.with()` — `static_rule_for<finite_guard, T>` is satisfied and the rule is stored as-is:

```cpp
auto model = vd::make_static_model<double>().with(vd::numeric::finite_t);
```

With `basic_model` the guard must be wrapped in an explicitly typed `vd::rule<T>`:

```cpp
auto model = vd::basic_model<double>{}.with(vd::rule<double>(vd::numeric::finite_t));
```

**`vd::predicate` does not work here.** It deduces `T` through `detail::first_arg_of`, which is defined as `first_arg_of<decltype(&Fn::operator())>` — and a templated `operator()` names an overload set, not a single function, so its address cannot be taken. The error is a hard one, raised outside the immediate context, so it is not detectable via SFINAE either. This is the same limitation already noted for generic lambdas in the `vd::predicate` documentation: *for generic lambdas or `std::function`, construct `rule<T>` directly*.

The wrap is therefore what pins the value type down — and since the guard is stateless, the same object can be pinned at several types at once:

```cpp
auto as_double = vd::basic_model<double>{}      .with(vd::rule<double>(vd::numeric::finite_t));
auto as_float  = vd::basic_model<float>{}       .with(vd::rule<float>(vd::numeric::finite_t));
auto as_int    = vd::basic_model<std::int32_t>{}.with(vd::rule<std::int32_t>(vd::numeric::finite_t));
```

### Failure message

```
Value is not finite: nan
```

Wrapped in `vd::member` / `vd::field` with a name, it becomes `Member <name> failed: Value is not finite: nan`.

---

## Adding a new numeric checker

`numeric_bounds` implements a single check — a range. For more specific needs (e.g. checking parity), a lambda is enough:

```cpp
auto even_rule = vd::predicate([](const int& v) { return v % 2 == 0; });
```

Or create a separate checker type satisfying `value_checker`:

```cpp
struct divisible_by {
    int divisor;
    bool operator()(int v) const { return v % divisor == 0; }
};

auto rule = vd::member(&MyStruct::count, divisible_by{3});
```
