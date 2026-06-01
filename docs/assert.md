# Assert module

**Заголовок:** `#include <vd.hxx>` (или `#include "assert/vd_assert.hxx"` напрямую)  
**Namespace:** `vd`

## Назначение

Assert module — Набор инструментов для контроля предусловий. Используется как внутри реализации (`vd::require` в `basic_bound_model`), так и в checker-ах (`vd::string_rules::detail::std_regex`). Может использоваться и в коде пользователя.

Ключевое свойство: при провале условия в сообщении об ошибке автоматически указываются **файл, строка и имя функции** — место вызова `require`, а не внутренности библиотеки. Это достигается захватом `std::source_location::current()` в параметре форматной строки через `consteval`-конструктор.

## API

### `concept contextually_bool`
Сервисный концепт для шаблонов `vd::require`, описывающий требование к произвольному объекту типа `T` такое, что произвольный объект типа `T` поддерживает `contextual bool conversion` - то есть может использоваться внутри условий (`if` и тернарный оператор) без явного приведения к `bool` типу при помощи `static_cast<>` или других механизмов.

### `vd::require`

```cpp
template<detail::contextually_bool Cond, typename... Args>
void vd::require(Cond&& condition, format_string fmt, Args&&... args);
```

Параметр condition может являться любым типом, допускающим контекстное приведение к `bool`.

Если `condition == false` — форматирует сообщение через `std::format`, выводит в `stderr` и вызывает `std::abort()`.

```cpp
vd::require(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require(value > 0, "Value must be positive, got {}", value);
vd::require(true, "This never fires");
```

Форматная строка — compile-time: тип аргументов проверяется при компиляции через `std::format_string<Args...>`.

**Вывод при ошибке:**
```
Assertion failed: Expected non-null pointer in foo
File: src/foo.cpp
Line: 42
Function: void foo()
```

### `vd::require<exception>`
```cpp
template<detail::contextually_bool Cond, typename ExceptionType, typename... Args>
    requires std::derived_from<ExceptionType, std::exception>
void vd::require(Cond&& condition, format_string fmt, Args&&... args);
```

Если `condition == false` — форматирует сообщение через `std::format` и бросает `ExceptionType`, конструируя его из отформатированной строки (передаётся в конструктор как `std::string`). Это означает, что `what()` исключения будет содержать именно текст сообщения — без строки, файла и функции (в отличие от abort-версии).

`ExceptionType` обязан наследоваться от `std::exception` и принимать `std::string` в конструктор.

```cpp
vd::require<std::logic_error>(ptr != nullptr, "Expected non-null pointer in {}", __func__);
vd::require<vd::validation_exception>(value > 0, "Value must be positive, got {}", value);
vd::require<std::runtime_error>(true, "This never fires");
```

**Вывод при ошибке:**
```
// exception.what() == "Expected non-null pointer in foo"
```

Никакого вывода в `stderr`. Место вызова (`source_location`) захватывается, но в тексте исключения не используется.

### `vd::require_callback`

```cpp
template<auto OnFailed, detail::contextually_bool Cond, typename... Args>
    requires std::invocable<decltype(OnFailed), std::string_view>
void vd::require_callback(Cond&& condition, format_string fmt, Args&&... args);
```

Вместо `abort()` вызывает переданный NTTP-callable с отформатированным сообщением. Полезно для тестирования и для встраивания в систему логирования.

```cpp
void my_logger(std::string msg) { std::cerr << "[ERROR] " << msg << "\n"; }

vd::require_callback<my_logger>(value > 0, "Bad value: {}", value);
```

`OnFailed` — шаблонный не-типовой параметр, поэтому callback разрешается в compile time без накладных расходов на `std::function`. Callback получает `std::string_view` на отформатированное сообщение.

## Внутреннее устройство

### `assert_format<Args...>`

Вспомогательная структура, которая одновременно хранит `std::format_string<Args...>` и `std::source_location`. Конструктор — `consteval`, что позволяет `std::source_location::current()` захватить место вызова `require`, а не место определения самой `assert_format`.

Трюк с `std::type_identity_t<Args>...` в сигнатуре `require` нужен для того, чтобы вывод типов `Args` шёл из trailing-аргументов, а не из форматной строки (иначе компилятор не может разрешить два независимых deduction на одни и те же `Args`):

```cpp
// Сигнатура require:
void require(Cond&& condition,
             details::assert_format<std::type_identity_t<Args>...> fmt_loc,
             Args&&... args);
//                    ^^^^^^^^^^^^^^^^ <- Args выводятся отсюда
//                                                          ^^^^ <- не отсюда
```

### `assert_fail`

`[[noreturn]]` функция, которая форматирует и печатает сообщение в `stderr`, затем вызывает `std::abort()`. Вынесена отдельно, чтобы снизить размер кода при инстанцировании `require` с разными наборами Args.

### `Debug` перегрузки

У каждой из функций `require`, `require<>`, `require_callback` существуют Debug-перегрузки `required`, `required<>`, `require_callbackd`, работающие только в сборках без объявленного символа `_NDEBUG` (с подчёркиванием, не `NDEBUG`).

```cpp
#ifndef _NDEBUG
// debug-only реализации
void required(Cond&& condition, format_string fmt, Args&&... args);
template<typename ExceptionType, ...> void required(...);
template<auto OnFailed, ...> void require_callbackd(...);
#else
// пустые no-op заглушки
#endif
```

Они не отличаются по механике работы от обычных функций внутри debug-сборок, но удаляются в release. В release-версии `require_callbackd` имеет ограничение `std::invocable<decltype(OnFailed), std::string>` (а не `std::string_view` как в debug) — это известная несогласованность в коде; заглушка всё равно ничего не вызывает.
