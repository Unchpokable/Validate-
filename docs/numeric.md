# Numeric module

**Заголовок:** `#include <vd.hxx>`  
**Файл реализации:** `src/models/vd_numeric.hxx`  
**Namespace:** `vd`

---

## Назначение

Модуль предоставляет `numeric_bounds<T>` — checker для числовых типов, реализующий `value_checker` концепт. Используется как второй аргумент в `vd::field` / `vd::member`.

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

### Концепт `numeric_compatible`

```cpp
// Определён в src/utils/vd_ctnextafter.hxx:
template<typename T>
concept generic_numer = std::integral<T> || std::floating_point<T>;

// Определён в src/models/vd_numeric.hxx:
template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept numeric_compatible = generic_numer<T> && arithmetic<T>;
```

Принимает: `int`, `double`, `float`, `uint8_t`, `int64_t` и т.д.  
Не принимает: `bool`, классы, указатели.

### Фабричные методы

| Метод | Семантика | Пример |
|-------|-----------|--------|
| `inclusive(min, max)` | `value >= min && value <= max` | `[0, 100]` |
| `exclusive(min, max)` | `value > min && value < max` | `(0, 100)` |
| `greater_than(min)` | `value > min` | `(5, +∞)` |
| `less_than(max)` | `value < max` | `(-∞, 10)` |
| `unbounded()` | всегда `true` | `(-∞, +∞)` |
| `outside_inclusive(lo, hi)` | `value <= lo \|\| value >= hi` | вне `[lo, hi]` |
| `outside_exclusive(lo, hi)` | `value < lo \|\| value > hi` | вне `(lo, hi)` |

### Реализация exclusive-границ

`exclusive`, `greater_than`, `less_than`, `outside_exclusive` реализованы через библиотечный `vd::ct_nextafter<T>` — `constexpr`-аналог `std::nextafter`, поддерживающий и целые числа, и числа с плавающей точкой:

```cpp
// exclusive(0.0, 1.0) создаёт bounds с:
min = ct_nextafter(0.0, std::numeric_limits<double>::max());    // 0.0 + epsilon
max = ct_nextafter(1.0, std::numeric_limits<double>::lowest()); // 1.0 - epsilon

// exclusive(0, 10) для int:
min = ct_nextafter(0, std::numeric_limits<int>::max());  // 1
max = ct_nextafter(10, std::numeric_limits<int>::min()); // 9
```

Это работает корректно как для `float`/`double`, так и для целочисленных типов.

---

## Примеры использования

```cpp
// Поле int в диапазоне [1, 100]
vd::member(&User::age, vd::int_bounds::inclusive(1, 100))

// Getter double строго больше нуля
vd::field(&Sensor::get_value, vd::double_bounds::greater_than(0.0))

// float строго меньше 1.0 (exclusive)
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

Готовые псевдонимы `basic_model<T>` для числовых типов:

```cpp
using int_model    = basic_model<std::int32_t>;
using double_model = basic_model<double>;
// ... и т.д.
```

Пример использования:
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

Возвращает правило, которое проверяет `std::isfinite(value)`. Применяется к полям с плавающей точкой, чтобы отфильтровать `NaN` и `inf`:

```cpp
auto model = vd::basic_model<Measurement>()
    .with(vd::member(&Measurement::value, vd::double_bounds::greater_than(0.0)))
    .with(vd::numeric::finite<double>());
```

---

## Добавление нового числового checker-а

`numeric_bounds` реализует одну проверку — диапазон. Для специфических нужд (например, проверка чётности) достаточно написать лямбду:

```cpp
auto even_rule = vd::predicate([](const int& v) { return v % 2 == 0; });
```

Или создать отдельный checker-тип, удовлетворяющий `value_checker`:

```cpp
struct divisible_by {
    int divisor;
    bool operator()(int v) const { return v % divisor == 0; }
};

auto rule = vd::member(&MyStruct::count, divisible_by{3});
```
