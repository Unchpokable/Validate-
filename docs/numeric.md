# Numeric module

**Header:** `#include <vd.hxx>`  
**Implementation file:** `src/models/vd_numeric.hxx`  
**Namespace:** `vd`

---

## Purpose

The module provides `numeric_bounds<T>` — a checker for numeric types, implementing the `value_checker` concept. Used as the second argument to `vd::field` / `vd::member`.

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

## `vd::numeric::finite()`

```cpp
namespace vd::numeric {
    template<numeric_compatible T>
    vd::rule<T> finite();
}
```

Returns a rule that checks `std::isfinite(value)`. Applied to floating-point fields to filter out `NaN` and `inf`:

```cpp
auto model = vd::basic_model<Measurement>()
    .with(vd::member(&Measurement::value, vd::double_bounds::greater_than(0.0)))
    .with(vd::numeric::finite<double>());
```

The `std::isfinite(const T& value)` rule is always `true` if `T` is an integral type.

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
