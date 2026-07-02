# Qt extensions module

**Заголовки:** `#include "ext/qt/vd_qtbase.hxx"` — Qt Base (QString, QProperty)  
**Файлы реализации:** `src/ext/qt/qtbase/vd_qstring.hxx`, `src/ext/qt/qtbase/vd_qstring.cxx`, `src/ext/qt/qtbase/vd_qproperty.hxx`  
**Namespace:** `vd::qt`, `vd::qt::string_rules`  
**Зависимость:** Qt 5/6 (QtCore), CTRE (`src/inline_deps/ctre.hpp`)

---

## Назначение

Qt-расширение добавляет checker-ы и фабрики правил, которые работают с Qt-типами напрямую — без конвертации в стандартные C++ типы. Все типы из этого модуля совместимы с `value_checker` концептом и используются с теми же `vd::member` / `vd::field` / `vd::predicate`, что и в базовой библиотеке.

```cpp
#include "ext/qt/vd_qtbase.hxx"

struct RegistrationForm : QObject {
    Q_OBJECT
    Q_PROPERTY(QString email READ email)
    Q_PROPERTY(int age READ age)
    // ...
};

auto form_model = vd::basic_model<RegistrationForm>()
    .with(vd::qt::qt_property<RegistrationForm>(
              "email", vd::qt::string_rules::email_like()))
    .with(vd::qt::qt_property<RegistrationForm>(
              "age", vd::int_bounds::inclusive(18, 120)));
```

---

## Структура модуля

```
src/ext/qt/
├── vd_qtbase.hxx          # Агрегирующий заголовок: qtbase-подмодули
│   ├── qtbase/
│   │   ├── vd_qstring.hxx # qstring_match, qregex_checker, фабрики
│   │   └── vd_qproperty.hxx # qt_property()
│
├── vd_qtwidgets.hxx       # Зарезервировано (пока пусто)
└── vd_qml.hxx             # Зарезервировано (пока пусто)
```

---

## `vd::qt::string_rules` — checker-ы для QString

Зеркало `vd::string_rules`, переписанное для `QStringView` / `QString`. API идентичен, поведение адаптировано под Qt-семантику.

### `qstring_match<Matcher>` — основной тип checker-а

```cpp
template<auto Matcher>
    requires qstring_matcher<Matcher>
struct qstring_match {
    enum class mode { include, exclude };
    mode match_mode = mode::include;
    std::string_view check_description = "string check failed";

    constexpr qstring_match() = default;
    constexpr qstring_match(mode m);
    constexpr qstring_match(mode m, std::string_view description);

    vd::result operator()(QStringView s) const;
};
```

Поле `check_description` передаётся в `vd::result::failed_rules` при провале. Фабричные функции задают осмысленное описание автоматически.

Аналог `vd::string_rules::string_match<Matcher>`, но оперирует `QStringView` вместо `std::string_view`. Поскольку `const QString&` неявно конвертируется в `QStringView`, checker совместим с `vd::member` на полях типа `QString`:

```cpp
struct Profile {
    QString username;
    QString bio;
};

auto model = vd::basic_model<Profile>()
    .with(vd::member(&Profile::username, vd::qt::string_rules::non_empty()))
    .with(vd::member(&Profile::bio,      vd::qt::string_rules::empty_or_whitespace()));
```

### Концепт `qstring_matcher`

```cpp
template<auto Matcher>
concept qstring_matcher =
    std::invocable<decltype(Matcher), QStringView>
    && std::convertible_to<std::invoke_result_t<decltype(Matcher), QStringView>, bool>;
```

NTTP-параметр `Matcher` — функция `bool(QStringView)` без состояния. Требования те же, что и у `string_matcher` в базовом модуле.

### `mode::include` / `mode::exclude`

`mode::exclude` инвертирует результат матчера — тот же механизм, что и в базовом `string_match`. Напрямую конструировать с `mode` нужно редко, т.к. для всех стандартных случаев есть фабрики.

---

## Фабричные функции

Все функции возвращают checker, совместимый с `value_checker`.

### `empty()`

```cpp
qstring_match<detail::empty_string> empty();
```

`true` только для `QString("")`. Использует `QStringView::isEmpty()`.

### `non_empty()`

```cpp
qstring_match<detail::non_empty_string> non_empty();
```

`true` для любой непустой строки, включая строки из одних пробелов.

### `empty_or_whitespace()`

```cpp
qstring_match<detail::empty_or_whitespace_string> empty_or_whitespace();
```

`true` если строка пуста или состоит только из символов, для которых `QChar::isSpace()` возвращает `true`.

**Отличие от std-версии:** базовый `empty_or_whitespace_string` проверяет ограниченный набор ASCII-пробелов (`' '`, `'\t'`, `'\n'`, `'\r'`, `'\f'`, `'\v'`). Qt-версия использует `QChar::isSpace()`, которая покрывает все Unicode-пробелы (U+00A0 NO-BREAK SPACE, U+2003 EM SPACE и т.д.), что соответствует Qt-идиомам при работе с интернациональным текстом.

### `email_like()`

```cpp
qstring_match<detail::email_like> email_like();
```

Минимальная эвристика: паттерн `^\S+@\S+\.\S+$` через CTRE. Строка конвертируется в UTF-8 (`QStringView::toUtf8()`) перед передачей в CTRE — те же compile-time паттерны, что в базовом модуле.

### `uri_like()`

```cpp
qstring_match<detail::uri_like> uri_like();
```

Паттерн `^\w+://\S+$` через CTRE. Та же логика конвертации в UTF-8.

---

## Правила длины: `min_length` / `max_length` / `length_in_between`

Зеркало std-версии из [string-rules.md](string-rules.md#правила-длины-строки-min_length--max_length--length_in_between), но без CharT-обобщения — `QString` внутри всегда UTF-16, отдельный шаблон по символьному типу не нужен:

```cpp
constexpr detail::min_length_t         min_length(std::size_t min_len);
constexpr detail::max_length_t         max_length(std::size_t max_len);
constexpr detail::length_in_between_t  length_in_between(std::size_t min_len, std::size_t max_len);
```

Каждый checker вызывает `QStringView::size()` и сравнивает с границами:

```cpp
struct max_length_t final {
    std::size_t max_len;
    constexpr max_length_t(std::size_t max_len);   // throws vd::assertion_exception если max_len == 0

    vd::result operator()(QStringView s) const;    // ошибка, если s.size() > max_len
};
```

```cpp
auto model = vd::basic_model<Profile>()
    .with(vd::member(&Profile::username, vd::qt::string_rules::min_length(3)))
    .with(vd::member(&Profile::bio,      vd::qt::string_rules::max_length(280)));
```

### Единица измерения: UTF-16 code units, не байты и не graphemes

`QStringView::size()` — это количество **UTF-16 code units** (2-байтовых машинных слов внутренней UTF-16 репрезентации `QString`). Это **не совпадает** с тем, что считает std-версия для `char`-строк (там — байты/UTF-8 code units): предельная длина, заданная числом для `vd::string_rules::max_length`, не переносится буквально на `vd::qt::string_rules::max_length` для того же текста — единицы разные. Как и в std-версии, суррогатные пары и составные grapheme-кластеры (эмодзи, комбинируемые диакритики) считаются не так, как «символы на экране».

### Валидация параметров конструктора

Как и в std-версии, конструкторы бросают `vd::assertion_exception` через `vd::ct_require` при некорректных аргументах (`max_len == 0`, `min_len == 0`, `max_len < min_len`) — это runtime-проверка, а не ошибка компиляции, несмотря на `constexpr`:

```cpp
vd::qt::string_rules::max_length(0);              // throw vd::assertion_exception
vd::qt::string_rules::length_in_between(10, 5);   // throw vd::assertion_exception
```

---

## `qregex_checker` — runtime-паттерн через QRegularExpression

```cpp
struct qregex_checker {
    enum class mode { include, exclude };
    QString pattern;
    mode match_mode = mode::include;

    vd::result operator()(QStringView s) const;
};
```

Аналог `vd::string_rules::regex_checker`, но использует `QRegularExpression` (PCRE2) вместо `std::regex`.

### Фабричная функция `regex()`

```cpp
qregex_checker regex(QString pattern);
```

```cpp
auto model = vd::basic_model<Form>()
    .with(vd::member(&Form::phone,
                     vd::qt::string_rules::regex(R"(\+7\d{10})")));
```

### Семантика совпадения

Используется **full-string match** (аналог `std::regex_match`, а не `std::regex_search`): совпадение засчитывается только если `capturedStart() == 0` и `capturedLength() == s.size()`. Паттерн должен описывать всю строку целиком.

### Невалидный паттерн

Если `QRegularExpression::isValid()` возвращает `false`, вызывается `vd::require(false, ...)` → `std::abort()` с диагностикой в `stderr`.

```cpp
auto bad = vd::qt::string_rules::regex("[invalid");
bad(QStringView{});  // abort: "Invalid regex pattern: [invalid"
```

### Преимущество над `std::regex`

`QRegularExpression` основан на PCRE2 и поддерживает полный Unicode из коробки, именованные группы, look-ahead/behind и прочие PCRE2-возможности, недоступные в `std::regex`.

---

## `vd::qt::qt_property` — валидация Q_PROPERTY

```cpp
template<typename T, typename Checker>
auto qt_property(const QString& prop_name, Checker checker) -> rule<T>;
```

Создаёт `rule<T>` для чтения Qt-свойства (объявленного через `Q_PROPERTY`) и проверки его значения checker-ом. `T` должен быть наследником `QObject`.

```cpp
struct Widget : QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(bool enabled READ isEnabled)
};

auto model = vd::basic_model<Widget>()
    .with(vd::qt::qt_property<Widget>(
              "text", vd::qt::string_rules::non_empty()))
    .with(vd::qt::qt_property<Widget>(
              "enabled", [](bool v) { return v; }));
```

### Вывод типа свойства

Тип значения свойства `PropT` выводится из первого аргумента `Checker` через `detail::first_arg_of<Checker>`. Это тот же трейт, что используется в базовом `vd::predicate`.

**Ограничение:** для generic-лямбд и `std::function` вывод типа невозможен. В таких случаях нужно явно создать `rule<T>`:

```cpp
vd::rule<Widget> r([](const Widget& w) {
    return w.property("count").toInt() > 0;
});
```

### Цепочка проверок внутри правила

Правило возвращает `vd::result` с ошибкой в следующих случаях (без abort, просто провал):

| Ситуация | Поведение |
|----------|-----------|
| `T` не является `QObject` | `vd::result::failed({"Object is not a QObject"})` |
| Свойство `prop_name` не существует | `vd::result::failed({"Property is not valid"})` |
| Значение `QVariant` нельзя сконвертировать в `PropT` | `vd::result::failed({"Property cannot be converted to expected type"})` |
| Checker возвращает провал | `vd::result::failed({...сообщение от checker-а...})` |

Имя свойства сохраняется как `QByteArray` (UTF-8) внутри замыкания — конвертация из `QString` происходит один раз при создании правила, не при каждой проверке.

---

## Подключение

Модуль включается через `<vd.hxx>` **автоматически**, когда CMake-опция `VD_EXTENSION_QT_BASE` установлена в `ON`. В этом случае компилятор получает макрос `VD_ENABLE_EXTENSION_QT_BASE`, и `vd_ext.hxx` подтягивает `vd_qtbase.hxx` в рамках основного заголовка.

```cmake
set(VD_EXTENSION_QT_BASE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Validate)
target_link_libraries(my_target PRIVATE Validate::vd Qt6::Core)
```

```cpp
#include <vd.hxx>  // Qt extensions уже внутри
```

При необходимости подмодули можно включить явно (например, в проектах без FetchContent):

```cpp
// Весь qtbase (QString + QProperty):
#include "ext/qt/vd_qtbase.hxx"

// Только QString-checker-ы:
#include "ext/qt/qtbase/vd_qstring.hxx"

// Только qt_property:
#include "ext/qt/qtbase/vd_qproperty.hxx"
```

---

## Планируемые подмодули

| Файл | Статус | Назначение |
|------|--------|------------|
| `vd_qtwidgets.hxx` | Зарезервирован | Checker-ы для QWidget-свойств |
| `vd_qml.hxx` | Зарезервирован | Интеграция с QML-контекстом |
