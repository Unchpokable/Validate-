# Assert module

**Заголовок:** `#include <vd.hxx>` (или `#include "assert/vd_assert.hxx"` напрямую)  
**Namespace:** `vd`

## Назначение

Assert module — внутренний инструмент библиотеки для контроля предусловий. Используется как внутри реализации (`vd::require` в `basic_bound_model`), так и в checker-ах (`vd::string_rules::detail::std_regex`). Может использоваться и в коде пользователя.

Ключевое свойство: при провале условия в сообщении об ошибке автоматически указываются **файл, строка и имя функции** — место вызова `require`, а не внутренности библиотеки. Это достигается захватом `std::source_location::current()` в параметре форматной строки через `consteval`-конструктор.

## API

### `vd::require`

```cpp
template<typename... Args>
void vd::require(bool condition, format_string fmt, Args&&... args);
```

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

### `vd::require_callback`

```cpp
template<auto OnFailed, typename... Args>
    requires std::invocable<decltype(OnFailed), std::string>
void vd::require_callback(bool condition, format_string fmt, Args&&... args);
```

Вместо `abort()` вызывает переданный NTTP-callable с отформатированным сообщением. Полезно для тестирования и для встраивания в систему логирования.

```cpp
void my_logger(std::string msg) { std::cerr << "[ERROR] " << msg << "\n"; }

vd::require_callback<my_logger>(value > 0, "Bad value: {}", value);
```

`OnFailed` — шаблонный не-типовой параметр, поэтому callback разрешается в compile time без накладных расходов на `std::function`.

## Внутреннее устройство

### `assert_format<Args...>`

Вспомогательная структура, которая одновременно хранит `std::format_string<Args...>` и `std::source_location`. Конструктор — `consteval`, что позволяет `std::source_location::current()` захватить место вызова `require`, а не место определения самой `assert_format`.

Трюк с `std::type_identity_t<Args>...` в сигнатуре `require` нужен для того, чтобы вывод типов `Args` шёл из trailing-аргументов, а не из форматной строки (иначе компилятор не может разрешить два независимых deduction на одни и те же `Args`):

```cpp
// Сигнатура require:
void require(bool condition,
             details::assert_format<std::type_identity_t<Args>...> fmt_loc,
             Args&&... args);
//                    ^^^^^^^^^^^^^^^^ <- Args выводятся отсюда
//                                                          ^^^^ <- не отсюда
```

### `assert_fail`

`[[noreturn]]` функция, которая форматирует и печатает сообщение в `stderr`, затем вызывает `std::abort()`. Вынесена отдельно, чтобы снизить размер кода при инстанцировании `require` с разными наборами Args.
