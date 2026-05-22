# Расширение библиотеки

Этот документ описывает, как добавлять новые checker-ы, новые типы моделей и как развивать существующие абстракции.

---

## Написание нового checker-а

Требование одно: тип должен быть callable `V -> bool` (удовлетворять `value_checker<Checker, V>`). Никакого наследования, макросов или регистрации не нужно.

### Вариант 1: лямбда

Самый простой случай. Используется прямо в месте создания правила:

```cpp
auto model = vd::basic_model<Product>()
    .with(vd::member(&Product::price, [](double v) { return v > 0 && v < 1e6; }))
    .with(vd::predicate([](const Product& p) { return !p.name.empty() && p.sku > 0; }));
```

### Вариант 2: структура со state

Когда checker параметризован runtime-данными:

```cpp
struct multiple_of {
    int divisor;
    bool operator()(int v) const noexcept { return v % divisor == 0; }
};

struct min_length {
    std::size_t n;
    bool operator()(std::string_view s) const noexcept { return s.size() >= n; }
};

auto model = vd::basic_model<Item>()
    .with(vd::member(&Item::quantity, multiple_of{5}))
    .with(vd::member(&Item::label,    min_length{3}));
```

### Вариант 3: `string_match<Matcher>` для строк без состояния

Когда matcher — stateless функция `bool(std::string_view)` и нужна поддержка `mode::include/exclude`:

```cpp
namespace my_rules {
    inline bool is_hex_color(std::string_view s) {
        if(s.size() != 7 || s[0] != '#') return false;
        for(int i = 1; i < 7; ++i)
            if(!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }
}

// Использование:
auto checker = vd::string_rules::string_match<my_rules::is_hex_color>{
    vd::string_rules::string_match<my_rules::is_hex_color>::mode::include
};

// Фабричная функция (рекомендуется для читаемости):
inline auto hex_color() {
    return vd::string_rules::string_match<my_rules::is_hex_color>{
        vd::string_rules::string_match<my_rules::is_hex_color>::mode::include
    };
}
```

---

## Добавление нового модуля checker-ов

По аналогии с `vd_numeric.hxx` и `vd_string_rules.hxx`:

1. Создайте `src/models/vd_<your_module>.hxx`
2. Добавьте `#include "models/vd_<your_module>.hxx"` в `src/vd_models.hxx`
3. Определите checker-типы в namespace `vd` или `vd::<your_namespace>`
4. Все функции-фабрики в `.hxx` должны быть `inline`

Шаблон нового модуля:

```cpp
#pragma once
#ifndef VD_MY_MODULE_HXX
#define VD_MY_MODULE_HXX

#include "vd_basic_model.hxx"
// ... другие нужные заголовки

namespace vd::my_rules
{
struct my_checker {
    /* параметры */
    bool operator()(/* тип значения */ v) const;
};

inline my_checker some_condition(/* параметры */)
{
    return { /* ... */ };
}
} // namespace vd::my_rules

#endif // VD_MY_MODULE_HXX
```

---

## Расширение `basic_model`

`basic_model<T>` намеренно минималистичен: только набор правил и `is_valid`. Если нужен специализированный тип модели (например, с кешированием результатов, именованными правилами или агрегацией ошибок), наследоваться не нужно — достаточно написать новый тип, который использует `rule<T>` внутри.

---

## Переход на `vd::result` (план на будущее)

Сейчас `rule<T>::operator()` и `basic_model::is_valid()` возвращают `bool`. Если в будущем потребуется возвращать диагностику (какое правило не сработало, почему), потребуется:

1. Ввести `vd::result` — тип, неявно конструируемый из `bool`:
   ```cpp
   struct result {
       bool ok;
       std::string reason;  // или что-то подобное
       
       /* implicit */ result(bool b) : ok(b) {}
       explicit operator bool() const { return ok; }
   };
   ```

2. Заменить в `rule<T>`:
   ```cpp
   std::function<bool(const T&)>  →  std::function<result(const T&)>
   ```

3. Изменить агрегацию в `basic_model::is_valid` — собирать failed results, а не просто возвращать `false`.

4. Существующие checker-ы (`numeric_bounds`, `string_match`, лямбды) остаются нетронутыми: неявная конструкция `result` из `bool` обеспечивает обратную совместимость.

Фабричные функции `field` / `member` / `predicate` при этом не изменяются.

---

## Тесты

При добавлении нового модуля создавайте отдельный тестовый файл в `tests/`:

```
tests/test_<module>.cxx
```

Зарегистрируйте его в `tests/CMakeLists.txt`:

```cmake
add_executable(test_my_module test_my_module.cxx)
target_link_libraries(test_my_module PRIVATE Validate::vd GTest::gtest_main)
gtest_discover_tests(test_my_module)
```

Принцип тестирования: если тест падает — это повод пересмотреть реализацию, а не подгонять тест.
