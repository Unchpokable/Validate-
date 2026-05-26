# Validate! — обзор библиотеки

## Что это

**Validate!** — header-only C++20 библиотека для декларативной валидации данных.
Идея: описать правила валидации объекта один раз через *модель*, а потом применять её к любому количеству объектов.

```cpp
struct User {
    std::string name;
    std::string email;
    int age;
    double salary;
};

auto user_model = vd::basic_model<User>()
    .with(vd::member(&User::name,   vd::string_rules::non_empty()))
    .with(vd::member(&User::email,  vd::string_rules::email_like()))
    .with(vd::member(&User::age,    vd::int_bounds::inclusive(18, 120)))
    .with(vd::member(&User::salary, vd::double_bounds::greater_than(0.0)));

User u{"Alice", "alice@example.com", 30, 75000.0};
bool ok = user_model.is_valid(u);   // true
```

## Требования

- C++20 (MSVC 19.29+, GCC 13+, Clang 14+)
- CMake 3.21+
- Для тестов: GTest (загружается автоматически через FetchContent)

## Сборка и тесты

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Структура файлов

```
src/
├── vd.hxx                     # Единственный публичный заголовок
├── vd_assert.hxx              # re-export: assert module
├── vd_models.hxx              # re-export: все модули моделей
├── vd_core.hxx                # (пока пустой, точка расширения)
├── vd_ext.hxx                 # (пока пустой, точка расширения)
│
├── assert/
│   └── vd_assert.hxx          # Реализация vd::require / vd::require_callback
│
├── models/
│   ├── vd_rule.hxx            # rule<T>, factory functions (field/member/predicate)
│   ├── vd_basic_model.hxx     # basic_model<T>, basic_bound_model<T>
│   ├── vd_numeric.hxx         # numeric_bounds<T> + type aliases
│   └── vd_string_rules.hxx    # string_match<>, regex_checker, factory functions
│
├── utils/
│   ├── vd_overload.hxx        # vd::overloaded (утилита для std::visit)
│   └── vd_sourceloc.hxx       # (утилиты source_location)
│
└── inline_deps/
    └── ctre.hpp               # Compile-Time Regular Expressions (CTRE)

ext/qt/
├── vd_qtbase.hxx              # Агрегирующий заголовок: qtbase-подмодули
├── vd_qtwidgets.hxx           # Зарезервировано (пусто)
├── vd_qml.hxx                 # Зарезервировано (пусто)
└── qtbase/
    ├── vd_qstring.hxx         # qstring_match<>, qregex_checker, фабрики
    ├── vd_qstring.cxx         # Реализация Qt string checker-ов
    └── vd_qproperty.hxx       # qt_property() для Q_PROPERTY

tests/
├── test_assert.cxx            # Тесты vd::require
├── test_models.cxx            # Тесты rule, basic_model, numeric_bounds
└── test_string_rules.cxx      # Тесты string_rules

docs/
├── overview.md                # Этот файл
├── assert.md                  # Assert module
├── models.md                  # rule, basic_model, basic_bound_model
├── numeric.md                 # numeric_bounds
├── string-rules.md            # string_rules
└── qt extensions.md           # Qt extensions (QString, QProperty)
```

## Ключевые концепции

| Концепт | Что это |
|---------|---------|
| `rule<T>` | Единичный предикат для объекта типа `T`. Хранит type-erased callable. |
| `basic_model<T>` | Коллекция `rule<T>`. Вызывает все правила при `is_valid()`. |
| `basic_bound_model<T>` | Связка модели с конкретным объектом. |
| `value_checker` | Концепт: callable `V -> bool`. Используется как второй аргумент фабрик `field`/`member`. |
| `numeric_bounds<T>` | Реализует `value_checker` для числовых типов. |
| `string_match<Matcher>` | Реализует `value_checker` для строк через NTTP-матчер. |
| `regex_checker` | Реализует `value_checker` для строк через `std::regex` с runtime-паттерном. |

## Как всё связано

```
vd::rule<T>
    ← создаётся через: vd::field(), vd::member(), vd::predicate()
    ← или напрямую: rule<T>([](const T&) { ... })

vd::basic_model<T>
    ← содержит: std::vector<rule<T>>
    ← строится через: .with(rule), .with({rules...}), .with(other_model)
    ← вызывает: is_valid(const T&) / is_valid(const T*)

value_checker (концепт)
    ← удовлетворяют: numeric_bounds<T>, string_match<Matcher>, regex_checker, лямбды
    ← используется: factory functions field() / member() в качестве второго аргумента
```

## Включение

Весь публичный API доступен через один заголовок:

```cpp
#include <vd.hxx>
```
