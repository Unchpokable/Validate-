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

Модуль **не включается** через `<vd.hxx>` автоматически — Qt-заголовки требуют наличия Qt в проекте сборки. Подключайте явно:

```cpp
// Весь qtbase (QString + QProperty):
#include "ext/qt/vd_qtbase.hxx"

// Только QString-checker-ы:
#include "ext/qt/qtbase/vd_qstring.hxx"

// Только qt_property:
#include "ext/qt/qtbase/vd_qproperty.hxx"
```

В CMakeLists.txt проект должен линковаться с `Qt::Core`:

```cmake
target_link_libraries(my_target PRIVATE Qt::Core)
```

---

## Планируемые подмодули

| Файл | Статус | Назначение |
|------|--------|------------|
| `vd_qtwidgets.hxx` | Зарезервирован | Checker-ы для QWidget-свойств |
| `vd_qml.hxx` | Зарезервирован | Интеграция с QML-контекстом |
