# Models module

**Заголовок:** `#include <vd.hxx>`  
**Файлы реализации:** `src/models/vd_rule.hxx`, `src/models/vd_basic_model.hxx`  
**Namespace:** `vd`

---

## Основная идея

Модуль строится на трёх уровнях:

```
rule<T>              — единичное правило: const T& -> vd::result
basic_model<T>       — набор правил + метод is_valid()
basic_bound_model<T> — модель, привязанная к конкретному объекту
```

Правило (`rule<T>`) — это type-erased обёртка над предикатом. Все варианты задания правила (через указатель на член, через getter, через лямбду) сводятся к одному внутреннему `std::function<vd::result(const T&)>`.

---

## `vd::rule<T>`

### Конструктор

```cpp
template<typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, rule>)
         && std::invocable<Fn, const T&>
         && std::convertible_to<std::invoke_result_t<Fn, const T&>, bool>
explicit rule(Fn&& fn);
```

Принимает любой callable с сигнатурой `const T& -> bool` или `const T& -> vd::result` (поскольку `vd::result` конвертируется в `bool`). Конструктор `explicit` — правила создаются через фабричные функции, а не неявным преобразованием.

```cpp
// Прямое создание (редко нужно вручную)
vd::rule<int> r([](const int& v) { return v > 0; });
```

### `operator()`

```cpp
vd::result operator()(const T& obj) const;
vd::result operator()(const T* obj) const;  // разыменовывает и вызывает reference-версию
```

Оба перегруза существуют, чтобы `basic_model::is_valid(const T*)` мог вызвать правило через указатель без лишнего кода.

---

## Фабричные функции

Это основной способ создавать правила. T выводится автоматически из типа указателя на член.

### `vd::field` — getter-метод + checker

```cpp
// С именем поля (используется в сообщениях об ошибках):
template<typename MemberPtr, typename Checker>
    requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;

// Без имени:
template<typename MemberPtr, typename Checker>
    requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;
```

Используется, когда значение для проверки нужно получить вызовом метода объекта.

```cpp
struct Sensor {
    double get_temperature() const;
};

auto rule = vd::field("temperature", &Sensor::get_temperature, vd::double_bounds::less_than(100.0));
// T = Sensor выводится из типа &Sensor::get_temperature
```

**Требование к методу:** должен быть `const` — объект передаётся как `const T&`.

### `vd::member` — поле + checker

```cpp
// С именем поля (используется в сообщениях об ошибках):
template<typename MemberPtr, typename Checker>
    requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;

// Без имени:
template<typename MemberPtr, typename Checker>
    requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;
```

Используется для прямого доступа к полю объекта.

```cpp
struct Point { double x, y; };

auto rule = vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 100.0));
// T = Point выводится из типа &Point::x
```

`vd::field` и `vd::member` намеренно разделены по имени, хотя реализация идентична. Различие в имени отражает намерение: *вычисленное значение* vs *хранимое поле*. Указание `field_name` добавляет контекст в сообщение об ошибке при провале правила.

### `vd::predicate` — произвольная лямбда

```cpp
template<typename Fn>
auto predicate(Fn fn) -> rule</*T выводится из первого аргумента Fn*/>;
```

T выводится из сигнатуры лямбды через `detail::first_arg_of`. Работает для не-generic, не-mutable лямбд (самый распространённый случай).

```cpp
auto rule = vd::predicate([](const Point& p) {
    return p.x >= 0 && p.y >= 0 && p.x * p.x + p.y * p.y < 10000.0;
});
```

Для generic-лямбд или случаев, где вывод T невозможен — создавайте `rule<T>` напрямую:

```cpp
vd::rule<MyType> r([](const MyType& v) { return v.is_valid_state(); });
```

---

## `vd::basic_model<T>`

Хранит `std::vector<rule<T>>` и применяет все правила последовательно.

### Построение модели

```cpp
// Пустая модель (принимает любой объект)
vd::basic_model<T> model;

// Через initializer_list в конструкторе
vd::basic_model<T> model({ rule1, rule2, rule3 });

// Через fluent builder
auto model = vd::basic_model<T>()
    .with(rule1)
    .with(rule2);

// Добавление набора правил
model.with({ rule3, rule4 });

// Слияние с другой моделью
auto combined = vd::basic_model<T>().with(base_model).with(extra_rule);
```

### `with()` — ref-qualified перегрузки

`with` имеет два варианта в зависимости от value category объекта модели:

```cpp
basic_model& with(rule<T>) &;     // lvalue: модифицирует на месте, возвращает *this
basic_model  with(rule<T>) &&;    // rvalue: добавляет правило и перемещает модель
```

**Почему это важно.** Паттерн `auto model = vd::basic_model<T>().with(...)` работает через rvalue-версию: временный объект перемещается в `model`, без копирования вектора правил. Если бы `with` всегда возвращал `basic_model&`, то `auto model = ...` потребовал бы копию, что проблематично в MSVC при определённых размерах захваченных данных в `std::function`.

```cpp
// rvalue path — move, не копирование:
auto model = vd::basic_model<T>().with(rule1).with(rule2);

// lvalue path — модификация на месте:
vd::basic_model<T> model;
model.with(rule1).with(rule2);  // возвращает basic_model&
```

### `is_valid()`

```cpp
vd::result is_valid(const T& object) const;
vd::result is_valid(const T* object) const;
```

Запускает **все** правила подряд и агрегирует результаты через `result::with_other()`. Не останавливается на первом провале — собирает сообщения от всех сработавших правил. Пустая модель возвращает `result::ok()`.

Если `object == nullptr`, возвращает `result(false)` без запуска правил.

### `die_if_failed()`

```cpp
void die_if_failed(const T& object) const;
void die_if_failed(const T* object) const;
```

Вызывает `is_valid()` и, если результат не валиден, бросает `vd::validation_exception`.

### `add_rule()`

```cpp
void add_rule(rule<T> rule);
```

Добавляет правило без возврата ссылки. Используется, когда fluent-цепочка не нужна.

---

## `vd::basic_bound_model<T>`

Связывает модель с конкретным объектом. Удобен, когда одну и ту же пару (модель, объект) нужно проверять несколько раз или передавать куда-то.

```cpp
vd::basic_model<Point> model = /* ... */;
Point p{3.0, 4.0};

auto bound = model.bind(p);
bool ok = bound.is_valid();  // эквивалентно model.is_valid(p)
```

### `is_valid()` / `die_if_failed()`

```cpp
vd::result is_valid() const;
void die_if_failed() const;
```

Делегируют в `m_model.is_valid(m_object)` и `m_model.die_if_failed(m_object)` соответственно.

### Важное ограничение: non-owning ссылка

`basic_bound_model<T>` хранит **неовладеющую ссылку** на модель (`const basic_model<T>&`). Модель должна пережить экземпляр `basic_bound_model`.

```cpp
// Корректно: model живёт дольше bound
auto model = vd::basic_model<Point>().with(...);
auto bound = model.bind(point);
bound.is_valid();  // OK

// Некорректно: dangling reference
auto bound = vd::basic_model<Point>().with(...).bind(point);
//           ^^^^^^^^^^^^^^^^^^^^^^^^ временная модель уничтожена до bind!
bound.is_valid();  // UB: ссылка висит
```

Если нужна независимость от времени жизни исходной модели — вызывайте `model.is_valid(point)` напрямую.

### `bind()` и nullptr

При передаче `nullptr` в `bind` срабатывает `vd::require` → `std::abort()`:

```cpp
model.bind(nullptr);  // abort с диагностическим сообщением
```

---

## Вспомогательные трейты

### `member_class_t<Ptr>`

Извлекает тип класса из любого указателя на член (поле или метод):

```cpp
member_class_t<double Point::*>             // -> Point
member_class_t<double(Point::*)() const>    // -> Point
```

Работает через единственную специализацию `V T::*` — паттерн совпадает как с указателями на данные (`double Point::*`), так и с указателями на методы (т.к. `double(Point::*)() const` тоже сводится к виду `V T::*`, где `V = double() const`).

### `detail::first_arg_of<Fn>`

Рекурсивный трейт, вытаскивающий тип первого аргумента из `operator() const` callable. Используется только внутри `vd::predicate`. Работает для не-generic, не-mutable лямбд. Mutable-лямбды и generic-лямбды не поддерживаются (нет `const`-квалификации у `operator()`).

### `value_checker<Checker, V>` concept

```cpp
template<typename Checker, typename V>
concept value_checker = std::invocable<Checker, V>
    && std::convertible_to<std::invoke_result_t<Checker, V>, bool>;
```

Используется как концепт-документация. Любой тип, удовлетворяющий этому концепту, может быть передан вторым аргументом в `vd::field` / `vd::member`. Поскольку `vd::result` имеет `operator bool()`, checker-ы, возвращающие `vd::result`, тоже удовлетворяют этому концепту.

---

## Свободные функции `validate_many`

Вспомогательные функции для валидации нескольких объектов сразу. Возвращают `bool` (не `vd::result`).

```cpp
// Произвольное число объектов одного типа (variadic):
template<typename T, typename... Args>
    requires(std::same_as<std::decay_t<Args>, T> && ...)
bool validate_many(const basic_model<T>& model, Args&&... objects);

// Вектор объектов по значению:
template<typename T>
bool validate_many(const basic_model<T>& model, const std::vector<T>& objects);

// Вектор указателей (non-const):
template<typename T>
    requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<T*>& object_ptrs);

// Вектор указателей (const):
template<typename T>
    requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<const T*>& object_ptrs);
```

Возвращает `true`, только если все объекты прошли валидацию.
