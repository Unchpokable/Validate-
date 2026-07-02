# String rules module

**Заголовок:** `#include <vd.hxx>`  
**Файлы реализации:** `src/models/vd_string_rules.hxx` (шаблонная часть), `src/models/vd_string_rules.cxx` (`regex_checker` и фабрика `regex()`, вынесены отдельно, т.к. не шаблонные и тянут `<regex>`)  
**Namespace:** `vd::string_rules`  
**Зависимость:** CTRE (compile-time regular expressions, `src/inline_deps/ctre.hpp`)

---

## Назначение

Модуль предоставляет checker-ы для строковых полей. Все checker-ы принимают `std::string_view` и возвращают `vd::result`, поэтому совместимы с `value_checker` концептом (который требует конвертируемости в `bool`) и работают с `vd::member` / `vd::field` при полях типа `std::string`.

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
    mode match_mode = mode::include;
    std::string_view check_description = "string check failed";

    constexpr string_match() = default;
    constexpr string_match(mode m);
    constexpr string_match(mode m, std::string_view description);

    vd::result operator()(std::string_view s) const;
};
```

Поле `check_description` используется как текст ошибки в `vd::result::failed_rules`, когда правило не срабатывает. Фабричные функции (`empty()`, `non_empty()` и т.д.) передают осмысленное описание автоматически.

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

## Обобщение по `CharT`: не только `std::string_view`

`string_match<Matcher>` формально по-прежнему шаблонизирован только по NTTP `Matcher` — `CharT` в списке шаблонных параметров структуры нет. Обобщение по типу символа достигается через **перегрузки `operator()`** на одной и той же структуре (`src/models/vd_string_rules.hxx`):

```cpp
// 1. std::string_view — фиксированный, всегда доступен:
vd::result operator()(std::string_view s) const;

// 2. Любой std::basic_string_view<CharT, Traits> с CharT != char
//    (wchar_t, char8_t, char16_t, char32_t) — если Matcher умеет его принять:
template<typename CharT, typename Traits>
requires(!std::same_as<CharT, char>) && /* Matcher invocable c этим view */
vd::result operator()(std::basic_string_view<CharT, Traits> s) const;

// 3. std::basic_string<CharT, Traits, Alloc> c CharT != char — форвардится в (2):
template<typename CharT, typename Traits, typename Alloc>
requires(!std::same_as<CharT, char>)
vd::result operator()(const std::basic_string<CharT, Traits, Alloc>& s) const
{
    return (*this)(std::basic_string_view<CharT, Traits>(s));
}
```

**Почему три перегрузки, а не один шаблон `template<typename CharT>`:** `char` намеренно вынесен в отдельную нешаблонную перегрузку (1), чтобы она никогда не конкурировала с шаблонной (2) — иначе `std::string_view` совпадал бы сразу с обеими и компилятор не мог бы выбрать лучшую перегрузку однозначно. Перегрузка (3) нужна отдельно, потому что `std::basic_string<CharT>` не выводится в `std::basic_string_view<CharT>` через template argument deduction (нет guide, который бы это сделал автоматически в данном контексте) — поэтому конверсия сделана явно внутри тела функции.

Сам `Matcher` при этом остаётся compile-time NTTP-функцией; какие символьные типы он умеет принимать, зависит только от его собственной сигнатуры (см. ниже).

### Что реально обобщено, а что — нет

| Фабрика | Работает с `wchar_t`/`char8_t`/`char16_t`/`char32_t`? |
|---|---|
| `empty()`, `non_empty()`, `empty_or_whitespace()` | ✅ да — соответствующие `detail`-матчеры сами шаблонны по `CharT` |
| `min_length()`, `max_length()`, `length_in_between()` | ✅ да — считают `s.size()` напрямую, не парсят содержимое |
| `email_like()`, `uri_like()` | ❌ нет — матчеры (`detail::email_like`, `detail::uri_like`) это обычные `constexpr bool(std::string_view)` на CTRE, не шаблонные; для них резолвится только перегрузка (1) |
| `regex()` | ❌ нет — `regex_checker` принимает только `std::string_view`, runtime `std::regex` |

```cpp
vd::string_rules::empty()(std::wstring_view(L""));           // OK
vd::string_rules::non_empty()(std::u16string_view(u"hi"));   // OK
vd::string_rules::non_empty()(std::u32string_view(U"hi"));   // OK
vd::string_rules::empty()(std::wstring(L""));                // OK, через перегрузку (3)

// vd::string_rules::email_like()(std::wstring_view(L"a@b.c"));  // не скомпилируется — нет перегрузки, принимающей wstring_view
```

---

## Правила длины строки: `min_length` / `max_length` / `length_in_between`

```cpp
constexpr detail::min_length_t         min_length(std::size_t min_len);
constexpr detail::max_length_t         max_length(std::size_t max_len);
constexpr detail::length_in_between_t  length_in_between(std::size_t min_len, std::size_t max_len);
```

Проверяют **длину строки в code units** — количество элементов `CharT` в `basic_string_view<CharT>`/`basic_string<CharT>`, а не количество grapheme-кластеров (пользовательски воспринимаемых символов). Для `char`-строк это фактически подсчёт байт; для `char16_t` — единиц UTF-16; для `char32_t` — кодовых точек. Для языков со сложными скриптами или эмодзи (комбинируемые последовательности, суррогатные пары и т.д.) это **не** совпадает с «числом символов на экране» — библиотека явно это не скрывает и предупреждает в doc-комментариях исходников.

```cpp
auto model = vd::basic_model<Profile>()
    .with(vd::field(&Profile::get_email, vd::string_rules::max_length(20)))
    .with(vd::member(&Profile::website,  vd::string_rules::min_length(5)))
    .with(vd::field(&Profile::get_name,  vd::string_rules::length_in_between(3, 10)));
```

Работают с тем же набором `CharT`, что и `empty`/`non_empty` — `std::string`, `std::wstring`, `std::u16string`, `std::u32string` и соответствующие `_view`.

### Валидация параметров конструктора

Конструкторы `min_length_t`/`max_length_t`/`length_in_between_t` — `constexpr`, но проверяют аргументы через `vd::ct_require<vd::assertion_exception>` (см. [assert.md](assert.md#vdct_require)), то есть **бросают исключение во время выполнения**, если параметры некорректны (это runtime-проверка над runtime-аргументами `std::size_t`, а не compile-time ошибка):

```cpp
vd::string_rules::max_length(0);              // throw vd::assertion_exception: "max_len must be positive"
vd::string_rules::min_length(0);               // throw vd::assertion_exception: "min_len must be positive"
vd::string_rules::length_in_between(0, 5);      // throw: "min_len must be positive"
vd::string_rules::length_in_between(10, 5);     // throw: "max_len must be greater than or equal to min_len"
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

    std::regex pattern;
    mode match_mode = mode::include;

    vd::result operator()(std::string_view s) const;
};
```

### Фабричная функция `regex()`

```cpp
regex_checker regex(std::string_view pattern);
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
bad("anything");  // -> abort: "Invalid regex pattern: [invalid. Error code: N"
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

Любой callable `std::string_view -> bool` (или `-> vd::result`) является валидным `value_checker` для строковых полей:

```cpp
// Через лямбду (возврат bool)
auto starts_with_http = [](std::string_view s) {
    return s.starts_with("http");
};
vd::member(&Config::base_url, starts_with_http)

// Через структуру с детализированной ошибкой (возврат vd::result)
struct min_length {
    std::size_t n;
    vd::result operator()(std::string_view s) const {
        if(s.size() >= n) return vd::result::ok();
        return vd::result::failed({std::format("string must be at least {} chars", n)});
    }
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
