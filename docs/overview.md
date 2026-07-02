# Validate! — обзор библиотеки

## Что это

**Validate!** — header-first C++20 библиотека для декларативной валидации данных.
Идея: описать правила валидации объекта один раз через *модель*, а потом применять её к любому количеству объектов.

Библиотека не полностью header-only: большая часть API — шаблонный код в заголовках, но нешаблонные части (`vd::detail::assert_fail`, `vd::result`, `regex_checker`/`regex()` из `string_rules`) вынесены в `.cxx`-файлы и собираются в статическую библиотеку `vd` (`add_library(vd STATIC ...)` в `CMakeLists.txt`). При использовании через `FetchContent` линковка (`target_link_libraries(... Validate::vd)`) обязательна — просто подключить `<vd.hxx>` без линковки недостаточно.

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
vd::result ok = user_model.check(u);   // ok.is_valid == true
```

## Требования

- C++20 (MSVC 19.29+, GCC 13+, Clang 14+)
- CMake 3.23+
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
├── vd_ext.hxx                 # re-export: Qt-расширения (VD_ENABLE_EXTENSION_QT_BASE / _QT_WIDGETS / _QT_QML)
│
├── assert/
│   ├── vd_assert.hxx          # Реализация vd::require / vd::require_callback / vd::ct_require
│   └── vd_assert.cxx          # Реализация vd::details::assert_fail
│
├── core/
│   ├── vd_result.hxx          # vd::result — декларация (данные + сигнатуры методов)
│   ├── vd_result.cxx          # vd::result — реализация методов (format/also/die_if_failed/…)
│   ├── vd_exception.hxx       # vd::validation_exception, vd::assertion_exception, vd_tagged_exception<>
│   ├── vd_not_null.hxx        # vd::not_null<T*> — ненулевой указатель-контракт
│   ├── vd_defines.hxx         # Макросы атрибутов: VD_LIKELY, VD_NODISCARD и т.д.
│   ├── vd_always_false.hxx    # Вспомогательный трейт always_false<T> для static_assert
│   └── vd_macro.hxx           # Макросы VD_MEMBER / VD_FIELD (только при VD_EXPORT_UNSAFE)
│
├── models/
│   ├── vd_rule.hxx            # rule<T>, value_checker concept, member_class_t trait
│   ├── vd_rule_factory.hxx    # Фабрики: vd::field(), vd::member(), vd::predicate(); vd::statics::field/member
│   ├── vd_basic_model.hxx     # basic_model<T>, basic_bound_model<T>, validate_many()
│   ├── vd_static_model.hxx    # static_model<T, Rules...> — compile-time модель (см. static-model.md)
│   ├── vd_memory.hxx          # vd::memory::not_null — готовый checker для pointer-like полей
│   ├── vd_numeric.hxx         # numeric_bounds<T> + type aliases
│   ├── vd_string_rules.hxx    # string_match<>, regex_checker, length-правила, factory functions
│   └── vd_string_rules.cxx    # Реализация regex_checker::operator() и regex() (std::regex — не шаблонные)
│
├── utils/
│   ├── vd_ctnextafter.hxx     # constexpr ct_nextafter<T> + concept generic_numer
│   ├── vd_overload.hxx        # vd::overloaded<Ts...> — helper для std::visit; в коде библиотеки не используется
│   └── vd_sourceloc.hxx       # vd::here() — consteval-обёртка над source_location::current(); нигде не вызывается
│
└── inline_deps/
    └── ctre.hpp               # Compile-Time Regular Expressions (CTRE)

ext/qt/
├── vd_qtbase.hxx              # Агрегирующий заголовок: qtbase-подмодули
├── vd_qtwidgets.hxx           # Зарезервировано (пусто)
├── vd_qml.hxx                 # Зарезервировано (пусто)
└── qtbase/
    ├── vd_qstring.hxx         # qstring_match<>, qregex_checker, length-правила, фабрики
    ├── vd_qstring.cxx         # Реализация Qt string checker-ов
    └── vd_qproperty.hxx       # qt_property() для Q_PROPERTY

tests/
├── test_assert.cxx            # Тесты vd::require / vd::ct_require
├── test_models.cxx            # Тесты rule, basic_model, numeric_bounds
├── test_static_model.cxx      # Тесты static_model<T, Rules...>
├── test_string_rules.cxx      # Тесты string_rules, включая CharT-обобщение и length-правила
├── test_not_null.cxx          # Тесты vd::not_null<T*>
├── test_integration.cxx       # Интеграционные тесты всей библиотеки
├── test_qt_property.cxx       # Тесты qt_property()
├── test_qt_qstring_rules.cxx  # Тесты vd::qt::string_rules
└── test_qt_integration.cxx    # Интеграционные тесты Qt-расширений

docs/
├── overview.md                # Этот файл
├── assert.md                  # Assert module
├── models.md                  # rule, basic_model, basic_bound_model
├── static-model.md            # static_model<T, Rules...> — compile-time модель
├── not_null.md                # not_null<T*> и vd::memory::not_null
├── numeric.md                 # numeric_bounds
├── string-rules.md            # string_rules
├── extending.md                # Как добавлять новые checker-ы и модули
└── qt extensions.md           # Qt extensions (QString, QProperty)
```

## Ключевые концепции

| Концепт | Что это |
|---------|---------|
| `rule<T>` | Единичный предикат для объекта типа `T`. Хранит type-erased callable `const T& -> vd::result`. |
| `basic_model<T>` | Коллекция `rule<T>` в `std::vector` (runtime). `check()` запускает все правила и собирает ошибки; `short_check()` — fail-fast вариант, останавливается на первой ошибке. |
| `basic_bound_model<T>` | Связка `basic_model` с конкретным объектом. |
| `static_model<T, Rules...>` | Compile-time модель: набор правил — часть типа (`std::tuple<Rules...>`), а не значения. `.with()` возвращает новый тип. См. [static-model.md](static-model.md). Эквивалента `basic_bound_model` для неё нет. |
| `vd::result` | Тип результата валидации: `bool is_valid` + `vector<string> failed_rules`. Конвертируется в `bool`. |
| `vd::not_null<T*>` | Обёртка над сырым указателем: гарантирует, что значение никогда не равно `nullptr`. Предназначена для использования в сигнатурах функций. Не путать с `vd::memory::not_null` — готовым checker-ом для полей внутри модели. |
| `value_checker` | Концепт: callable `V -> bool` (или `V -> vd::result`, т.к. он конвертируется в `bool`). Используется как второй аргумент фабрик `field`/`member` — проверяет **значение поля**. |
| `static_rule_for<Rule, T>` | Концепт для `static_model::with()`: callable `const T& -> bool \| vd::result`. Структурно похож на `value_checker`, но проверяет **весь объект**, а не значение поля. |
| `numeric_bounds<T>` | Реализует `value_checker` для числовых типов. |
| `string_match<Matcher>` | Реализует `value_checker` для строк через NTTP-матчер; обобщён по `basic_string_view<CharT>`/`basic_string<CharT>`. |
| `regex_checker` | Реализует `value_checker` для строк через `std::regex` с runtime-паттерном (только `std::string_view`). |

## Как всё связано

```
vd::rule<T>
    ← создаётся через: vd::field(), vd::member(), vd::predicate()
    ← или напрямую: rule<T>([](const T&) { return vd::result::ok(); })

vd::basic_model<T>
    ← содержит: std::vector<rule<T>>
    ← строится через: .with(rule), .with({rules...}), .with(other_model)
    ← вызывает: check(const T&) / check(const T*) → vd::result (все правила, все ошибки)
    ←           short_check(const T&)              → vd::result (fail-fast: первая ошибка)

vd::static_model<T, Rules...>
    ← содержит: std::tuple<Rules...> (правила — часть типа, не значения)
    ← строится через: make_static_model<T>().with(rule) — каждый .with() меняет тип модели
    ← правила: vd::field/member/predicate (rule<T>), vd::statics::field/member (без std::function), сырые лямбды
    ← check() / short_check() — та же семантика, что у basic_model, но через fold-выражения по кортежу

vd::result
    ← возвращается из: check(), short_check(), rule::operator()(), checker-ов
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
