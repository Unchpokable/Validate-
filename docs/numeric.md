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

    bool operator()(const T& value) const;

    static numeric_bounds<T> inclusive(T min, T max);
    static numeric_bounds<T> exclusive(T min, T max);
    static numeric_bounds<T> greater_than(T min);
    static numeric_bounds<T> less_than(T max);
    static numeric_bounds<T> unbounded();
};
```

### Концепт `numeric_compatible`

```cpp
concept numeric_compatible = (std::integral<T> || std::floating_point<T>)
                           && std::is_arithmetic_v<T>;
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

### Реализация exclusive-границ

`exclusive`, `greater_than` и `less_than` реализованы через `std::nextafter` — следующее представимое значение в направлении нужной границы:

```cpp
// exclusive(0.0, 1.0) создаёт bounds с:
min = std::nextafter(0.0, std::numeric_limits<double>::max());    // 0.0 + epsilon
max = std::nextafter(1.0, std::numeric_limits<double>::lowest()); // 1.0 - epsilon
```

Это работает корректно для `float` и `double`. Для **целочисленных типов** `std::nextafter` не определён, поэтому `exclusive` / `greater_than` / `less_than` на целых числах не будут компилироваться — используйте `inclusive` со сдвигом границы вручную.

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
