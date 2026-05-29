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
vd::result ok = user_model.is_valid(u);   // ok.is_valid == true
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
├── vd_core.hxx                # re-export: vd_result, vd_exception, vd_defines; условно vd_macro
├── vd_ext.hxx                 # re-export: Qt-расширения (при наличии Qt)
│
├── assert/
│   ├── vd_assert.hxx          # Реализация vd::require / vd::require_callback
│   └── vd_assert.cxx          # Реализация vd::details::assert_fail
│
├── core/
│   ├── vd_result.hxx          # vd::result — тип результата валидации
│   ├── vd_exception.hxx       # vd::validation_exception и vd_tagged_exception<>
│   ├── vd_defines.hxx         # Макросы атрибутов: VD_LIKELY, VD_NODISCARD и т.д.
│   ├── vd_always_false.hxx    # Вспомогательный трейт always_false<T> для static_assert
│   └── vd_macro.hxx           # Макросы VD_MEMBER / VD_FIELD (только при VD_EXPORT_UNSAFE)
│
├── models/
│   ├── vd_rule.hxx            # rule<T>, value_checker concept, member_class_t trait
│   ├── vd_rule_factory.hxx    # Фабрики: vd::field(), vd::member(), vd::predicate()
│   ├── vd_basic_model.hxx     # basic_model<T>, basic_bound_model<T>, validate_many()
│   ├── vd_numeric.hxx         # numeric_bounds<T> + type aliases
│   └── vd_string_rules.hxx    # string_match<>, regex_checker, factory functions
│
├── utils/
│   └── vd_ctnextafter.hxx     # constexpr ct_nextafter<T> + concept generic_numer
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
├── test_integration.cxx       # Интеграционные тесты всей библиотеки
└── test_qt_integration.cxx    # Интеграционные тесты Qt-расширений

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
| `rule<T>` | Единичный предикат для объекта типа `T`. Хранит type-erased callable `const T& -> vd::result`. |
| `basic_model<T>` | Коллекция `rule<T>`. Запускает все правила при `is_valid()` и собирает ошибки. |
| `basic_bound_model<T>` | Связка модели с конкретным объектом. |
| `vd::result` | Тип результата валидации: `bool is_valid` + `vector<string> failed_rules`. Конвертируется в `bool`. |
| `value_checker` | Концепт: callable `V -> bool` (или `V -> vd::result`, т.к. он конвертируется в `bool`). Используется как второй аргумент фабрик `field`/`member`. |
| `numeric_bounds<T>` | Реализует `value_checker` для числовых типов. |
| `string_match<Matcher>` | Реализует `value_checker` для строк через NTTP-матчер. |
| `regex_checker` | Реализует `value_checker` для строк через `std::regex` с runtime-паттерном. |

## Как всё связано

```
vd::rule<T>
    ← создаётся через: vd::field(), vd::member(), vd::predicate()
    ← или напрямую: rule<T>([](const T&) { return vd::result::ok(); })

vd::basic_model<T>
    ← содержит: std::vector<rule<T>>
    ← строится через: .with(rule), .with({rules...}), .with(other_model)
    ← вызывает: is_valid(const T&) / is_valid(const T*) → vd::result (все правила, не fail-fast)

vd::result
    ← возвращается из: is_valid(), rule::operator()(), checker-ов
    ← методы: format(), short_format(), die_if_failed(), operator bool()

value_checker (концепт)
    ← удовлетворяют: numeric_bounds<T>, string_match<Matcher>, regex_checker, лямбды
    ← используется: factory functions field() / member() в качестве второго аргумента
```

## Включение

Весь публичный API доступен через один заголовок:

```cpp
#include <vd.hxx>
```
