# not_null module

**Заголовок:** `#include <vd.hxx>` (транзитивно через `vd_models.hxx`)  
**Файл реализации:** `src/core/vd_not_null.hxx`  
**Namespace:** `vd`

---

## Назначение

`vd::not_null<T*>` — обёртка над сырым указателем, которая выражает предусловие «указатель никогда не равен `nullptr`» непосредственно в сигнатуре функции. Принцип: если правило нарушено, ошибка фиксируется в момент передачи аргумента, а не где-то внутри функции.

```cpp
// Без not_null — нарушение обнаружится (в лучшем случае) при разыменовании внутри
void process(Widget* w) {
    w->do_something();  // UB если w == nullptr
}

// С not_null — нарушение обнаруживается на вызывающей стороне
void process(vd::not_null<Widget*> w) {
    w->do_something();  // гарантированно безопасно
}
```

---

## Определение

```cpp
template<typename T>
requires std::is_pointer_v<T>
struct not_null final {
    using value_type         = std::remove_cv_t<std::remove_pointer_t<T>>;
    using pointer_type       = T;
    using const_pointer_type = const value_type*;
    using object_type        = std::remove_pointer_t<T>;
    using reference_type     = object_type&;
    using const_reference_type = const value_type&;
};
```

Принимает только типы-указатели (`T*`, `const T*`). `not_null<int>` не компилируется.

---

## Конструирование

### Из nullptr — запрещено

```cpp
constexpr not_null(std::nullptr_t) = delete;
```

Передача `nullptr` напрямую не компилируется:

```cpp
vd::not_null<int*> p = nullptr;  // ошибка компиляции
```

### Из ненулевого указателя

```cpp
constexpr not_null(pointer_type ptr);
```

В `constexpr`-контексте проверяет `ptr != nullptr` статически — нарушение является ошибкой компиляции. В runtime-контексте при `ptr == nullptr` вызывается `std::terminate()` с диагностическим сообщением в `stderr`:

```
not_null cannot be constructed with nullptr
```

### Ковариантное копирование

```cpp
template<typename U>
requires std::is_convertible_v<U, T>
constexpr not_null(const not_null<U>& other) noexcept;
```

Позволяет неявно приводить `not_null<Derived*>` к `not_null<Base*>`.

---

## Операторы

```cpp
constexpr reference_type operator*()  const noexcept;  // разыменование
constexpr pointer_type   operator->() const noexcept;  // доступ к члену
constexpr operator pointer_type()     const noexcept;  // неявное приведение к T*

constexpr pointer_type get() const noexcept;           // явное получение указателя

friend constexpr auto operator<=>(not_null, not_null) = default;
friend constexpr bool operator==(not_null, not_null)  = default;
```

`not_null<T*>` прозрачно конвертируется в `T*`, поэтому его можно передавать в функции, ожидающие сырой указатель, без явного вызова `get()`.

---

## Владение

`not_null<T*>` **не владеет** объектом. Это ненулевой наблюдатель, аналогичный сырому указателю по семантике владения. Для передачи владения используйте `std::unique_ptr` или `std::shared_ptr`; `not_null` применяется, когда указатель заимствован у вызывающего кода.

---

## Типичные паттерны

### Параметр функции

```cpp
void render(vd::not_null<Renderer*> r, vd::not_null<const Scene*> scene) {
    r->draw(*scene);
}
```

### Поле класса

```cpp
class View {
    vd::not_null<Model*> m_model;
public:
    explicit View(vd::not_null<Model*> model) : m_model(model) {}
};
```

### Совместимость с basic_model

Внутри библиотеки `not_null` используется в `basic_model::bind()` и `basic_bound_model::bind()` — эти методы принимают `not_null<const T*>`, явно документируя, что `nullptr` не является допустимым аргументом:

```cpp
auto bound = model.bind(vd::not_null<const Point*>(&point));
```

При передаче сырого ненулевого указателя компилятор строит `not_null` неявно.

---

## Сравнение с gsl::not_null

`vd::not_null<T>` намеренно минималистичен. В отличие от `gsl::not_null`:

- нет поддержки умных указателей (`unique_ptr`, `shared_ptr`)
- нет `[[gsl::Owner]]` / `[[gsl::Pointer]]` аннотаций
- нет зависимости от GSL

Библиотека ориентирована на сценарии передачи заимствованных сырых указателей в контексте валидации.
