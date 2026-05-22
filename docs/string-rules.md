# String rules module

**Заголовок:** `#include <vd.hxx>`  
**Файл реализации:** `src/models/vd_string_rules.hxx`  
**Namespace:** `vd::string_rules`  
**Зависимость:** CTRE (compile-time regular expressions, `src/inline_deps/ctre.hpp`)

---

## Назначение

Модуль предоставляет checker-ы для строковых полей. Все checker-ы принимают `std::string_view` и возвращают `bool`, поэтому совместимы с `value_checker` концептом и работают с `vd::member` / `vd::field` при полях типа `std::string`.

```cpp
auto model = vd::basic_model<User>()
    .with(vd::member(&User::name,    vd::string_rules::non_empty()))
    .with(vd::member(&User::email,   vd::string_rules::email_like()))
    .with(vd::member(&User::website, vd::string_rules::uri_like()));
```

---

## `string_match<Matcher>` — основной тип checker-а

```cpp
template<auto Matcher>
    requires string_matcher<Matcher>
struct string_match {
    enum class mode { include, exclude };
    mode match_mode;

    bool operator()(std::string_view s) const;
};
```

### Параметр шаблона `Matcher`

`Matcher` — **NTTP** (non-type template parameter): compile-time значение, вызываемое как `Matcher(string_view)`. На практике это функция `bool(std::string_view)` из пространства `detail`.

`string_matcher` концепт:
```cpp
template<auto Matcher>
concept string_matcher =
    std::invocable<decltype(Matcher), std::string_view>
    && std::convertible_to<std::invoke_result_t<decltype(Matcher), std::string_view>, bool>;
```

### `mode::include` / `mode::exclude`

`mode::exclude` инвертирует результат матчера — позволяет записать «не пустая строка» без отдельной функции (хотя для удобства `non_empty()` существует как отдельная фабрика).

```cpp
// Эквивалентные записи:
vd::string_rules::non_empty()
// и вручную:
vd::string_rules::string_match<vd::string_rules::detail::empty_string>{
    vd::string_rules::string_match<...>::mode::exclude
}
```

---

## Фабричные функции

Все функции возвращают checker, совместимый с `value_checker`.

### `empty()`

```cpp
string_match<detail::empty_string> empty();
```

Возвращает `true` только для пустой строки `""`. Whitespace-only строки (`" "`, `"\t"`) **не** считаются пустыми.

### `non_empty()`

```cpp
string_match<detail::non_empty_string> non_empty();
```

Возвращает `true` для любой непустой строки, включая строки из одних пробелов.

### `empty_or_whitespace()`

```cpp
string_match<detail::empty_or_whitespace_string> empty_or_whitespace();
```

Возвращает `true` если строка пуста или состоит только из символов `' '`, `'\t'`, `'\n'`, `'\r'`, `'\f'`, `'\v'`.

### `email_like()`

```cpp
string_match<detail::email_like> email_like();
```

Минимальная эвристика: паттерн `^\S+@\S+\.\S+$` через CTRE. Проверяет наличие `@`, домена и точки. Полноценная RFC 5322-валидация этой функцией **не предполагается**.

Примеры:
```
"user@example.com"    → true
"a@b.c"               → true
"notanemail"          → false
"user@"               → false
"@domain.com"         → false (нет локальной части — \S+ не совпадёт)
```

### `uri_like()`

```cpp
string_match<detail::uri_like> uri_like();
```

Минимальная эвристика: паттерн `^\w+://\S+$` через CTRE. Требует наличия схемы и `://`. Полноценная URI-валидация не предполагается.

Примеры:
```
"https://example.com"  → true
"ftp://files.org/path" → true
"example.com"          → false
"://bad"               → false
```

---

## `regex_checker` — runtime-паттерн

```cpp
struct regex_checker {
    enum class mode { include, exclude };

    std::string pattern;
    mode match_mode = mode::include;

    bool operator()(std::string_view s) const;
};
```

### Фабричная функция `regex()`

```cpp
regex_checker regex(std::string pattern);
```

Создаёт checker, который проверяет строку через `std::regex_match` (т.е. паттерн должен совпасть со **всей** строкой, а не с подстрокой).

```cpp
auto model = vd::basic_model<Form>()
    .with(vd::member(&Form::postal_code, vd::string_rules::regex(R"(\d{5}(-\d{4})?)")));
```

### Почему `regex_checker` отдельный тип, а не `string_match<...>`

`string_match<Matcher>` использует NTTP-параметр `auto Matcher`. В C++20 NTTP может быть функцией (указателем на функцию), структурой с `constexpr`-полями, но **не capturing lambda** и не callable с runtime-состоянием.

Паттерн `std::regex` — это runtime-данные (строка). Поэтому `regex_checker` хранит паттерн как `std::string` в поле, а не в параметре шаблона. Тип остаётся полноценным `value_checker` через свой `operator()(std::string_view)`.

### Невалидный паттерн

При невалидном паттерне `std::regex` бросает `std::regex_error`. Это исключение перехватывается и транслируется в `vd::require(false, ...)` → `std::abort()` с диагностическим сообщением в `stderr`.

```cpp
auto bad = vd::string_rules::regex("[invalid");
bad("anything");  // -> abort: "Invalid regex pattern: [invalid"
```

---

## Использование с `std::string`-полями

`string_match::operator()` и `regex_checker::operator()` принимают `std::string_view`. `std::string` неявно конвертируется в `std::string_view`, поэтому поля типа `std::string` работают без дополнительных конвертаций:

```cpp
struct User { std::string email; };

// std::invoke(&User::email, obj) возвращает const std::string&
// Checker принимает std::string_view — неявная конверсия
vd::member(&User::email, vd::string_rules::email_like())
```

---

## Написание собственного string checker-а

Любой callable `std::string_view -> bool` является валидным `value_checker` для строковых полей:

```cpp
// Через лямбду
auto starts_with_http = [](std::string_view s) {
    return s.starts_with("http");
};
vd::member(&Config::base_url, starts_with_http)

// Через структуру
struct min_length {
    std::size_t n;
    bool operator()(std::string_view s) const { return s.size() >= n; }
};
vd::member(&Post::body, min_length{10})
```

Для использования с `string_match<Matcher>` (с поддержкой `mode::include/exclude`) необходима функция `bool(std::string_view)` без состояния (чтобы быть NTTP):

```cpp
namespace my_matchers {
    bool starts_with_http(std::string_view s) { return s.starts_with("http"); }
}

// string_match с exclude-режимом:
vd::string_rules::string_match<my_matchers::starts_with_http>{
    vd::string_rules::string_match<my_matchers::starts_with_http>::mode::exclude
}
// Проверяет: НЕ начинается с "http"
```

---

## Зависимость CTRE

`email_like()` и `uri_like()` используют CTRE для compile-time regexp-компиляции. CTRE — header-only библиотека, расположена в `src/inline_deps/ctre.hpp`. Остальные функции модуля CTRE не требуют.
