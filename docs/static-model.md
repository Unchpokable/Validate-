# Static model module

**Заголовок:** `#include <vd.hxx>`
**Файлы реализации:** `src/models/vd_static_model.hxx`, фабрики `vd::statics::field` / `vd::statics::member` в `src/models/vd_rule_factory.hxx`
**Namespace:** `vd`, `vd::statics`

---

## Назначение

`vd::static_model<T, Rules...>` — второй тип модели в библиотеке, наряду с `vd::basic_model<T>` (см. [models.md](models.md)). Идея та же: набор правил + `check()`/`short_check()`. Разница — где живёт этот набор.

`basic_model<T>` хранит правила в `std::vector<rule<T>>`: каждое правило — это `rule<T>`, обёртка вокруг `std::function<vd::result(const T&)>`. Это удобно (правила можно добавлять во время выполнения, хранить модели в контейнерах и т.д.), но каждое правило, созданное через `vd::field`/`vd::member`/`vd::predicate`, почти наверняка означает аллокацию под `std::function`.

`static_model<T, Rules...>` хранит правила в `std::tuple<Rules...>` — набор правил является частью **типа** модели, а не её значения. Если тип правила — это захватывающая лямбда или простой функтор без состояния, компилятор может целиком развернуть проверку без единой аллокации на куче. Это тот случай, когда нужен полностью статический, heap-alloc-free конвейер валидации (например, в hot path или в embedded-контексте), а полный набор правил известен на этапе компиляции.

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
                 .with(vd::statics::member(&Point::y, vd::double_bounds::inclusive(0.0, 100.0)))
                 .with([](const Point& p) { return p.x != p.y; });

vd::result r = model.check(point);
```

---

## Сравнение с `basic_model<T>`

| Аспект | `basic_model<T>` | `static_model<T, Rules...>` |
|---|---|---|
| Хранилище правил | `std::vector<rule<T>>` (runtime, type-erased через `std::function`) | `std::tuple<Rules...>` (compile-time, каждое правило — отдельный тип) |
| Набор правил — часть чего | значения (можно менять в рантайме) | **типа** (`.with()` меняет тип модели) |
| `with()` на lvalue | мутирует объект на месте, возвращает `basic_model&` | мутации на месте нет вообще — только `const&`/`&&` перегрузки, обе возвращают **новый** `static_model<T, Rules..., NewRule>` |
| `add_rule()` | есть | нет (невозможен в принципе — набор правил фиксирован в типе) |
| Аллокации на куче | почти всегда (каждое правило — `std::function`) | не обязательны, если использовать `vd::statics::field`/`vd::statics::member`/сырые лямбды |
| `constexpr` | нет | конструкторы и `with()` — `constexpr`; `check()`/`short_check()` — нет (см. «Ограничения» ниже) |
| Bound-модель (`.bind()`) | `basic_bound_model<T>` | отсутствует — эквивалента нет |

---

## `vd::static_model<T, Rules...>`

```cpp
template<typename Rule, typename T>
concept static_rule_for = std::invocable<Rule, const T&>
    && (std::convertible_to<std::invoke_result_t<Rule, const T&>, vd::result>
        || std::convertible_to<std::invoke_result_t<Rule, const T&>, bool>);

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>) && (static_rule_for<Rules, T> && ...)
struct static_model final {
    using value_type           = T;
    using const_reference_type = const T&;
    using const_pointer_type   = const T*;

    constexpr static_model() requires(sizeof...(Rules) == 0) = default;
    constexpr explicit static_model(Rules... rules) requires(sizeof...(Rules) > 0);

    [[nodiscard]] vd::result check(const_reference_type object) const;
    [[nodiscard]] vd::result check(const_pointer_type object) const;        // nullptr -> result(false)

    [[nodiscard]] vd::result short_check(const_reference_type object) const;
    [[nodiscard]] vd::result short_check(const_pointer_type object) const;  // nullptr -> result(false)

    void die_if_failed(const_reference_type object) const;
    void die_if_failed(const_pointer_type object) const;

    template<typename NewRule> requires static_rule_for<NewRule, T>
    [[nodiscard]] constexpr auto with(NewRule new_rule) const& -> static_model<T, Rules..., NewRule>;

    template<typename NewRule> requires static_rule_for<NewRule, T>
    [[nodiscard]] constexpr auto with(NewRule new_rule) && -> static_model<T, Rules..., NewRule>;

private:
    std::tuple<Rules...> m_rules;
};

template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
constexpr auto make_static_model() -> static_model<T>;   // пустая static_model<T>
```

### `static_rule_for<Rule, T>` vs `value_checker<Checker, V>` — зачем два похожих концепта

Оба концепта структурно идентичны («callable → bool или vd::result»), но описывают разные роли в конвейере:

- `value_checker<Checker, V>` (в `vd_rule.hxx`) — проверяет **значение поля**: `V -> bool | vd::result`. Это то, что передаётся вторым аргументом в `vd::field`/`vd::member` (например, `vd::double_bounds::inclusive(...)`, `vd::string_rules::non_empty()`).
- `static_rule_for<Rule, T>` — проверяет **весь объект**: `const T& -> bool | vd::result`. Это то, что принимает `static_model::with()`.

Разделение оставляет две ответственности типобезопасно различимыми: checker поля не спутать с правилом объекта, даже если структурно оба выглядят как «что-то вызываемое, возвращающее bool». `basic_model` не нуждается в отдельном одноимённом концепте — там роль правила уже закрыта самим типом `rule<T>`.

### Пустая модель и `nullptr`

Модель без правил (`sizeof...(Rules) == 0`, создаётся через `make_static_model<T>()`) всегда валидна для любого объекта — но не для `nullptr`: `check(nullptr)`/`short_check(nullptr)` возвращают `result(false)` без обращения к правилам, как и у `basic_model`.

### Иммутабельность `with()`

Каждый вызов `.with(rule)` не модифицирует существующую модель — он **создаёт новую** модель с типом `static_model<T, Rules..., NewRule>`. Исходный объект (если вызван через `const&`) остаётся неизменным и валидным:

```cpp
auto base = vd::make_static_model<Point>()
                .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

auto extended = base.with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

// base проверяет только x, extended — x и y. base не изменился.
```

Это прямое следствие того, что набор правил — часть типа: у `basic_model<T>` есть один и тот же тип независимо от количества правил, поэтому мутация на месте возможна и естественна; у `static_model<T, Rules...>` добавление правила меняет сам тип объекта, поэтому «мутация на месте» синтаксически невозможна — вместо неё `with()` всегда возвращает новое значение (нового типа).

---

## Построение правил

`static_model` принимает любой callable, удовлетворяющий `static_rule_for<Rule, T>` — представление правила не фиксировано, в одной модели можно свободно смешивать три стиля.

### 1. Обычные `vd::rule<T>` (через `vd::field` / `vd::member` / `vd::predicate`)

Те же фабрики, что и для `basic_model`, работают и здесь — но каждое такое правило по-прежнему оборачивается в `rule<T>`/`std::function`, то есть аллокация на куче не устраняется:

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                 .with(vd::predicate([](const Point& p) { return p.x != p.y; }));
```

### 2. `vd::statics::field` / `vd::statics::member` — heap-alloc-free фабрики

Используют ту же внутреннюю реализацию (`detail::field_impl` / `detail::member_impl`), что и `vd::field`/`vd::member`, но возвращают «сырую» лямбду напрямую (через `auto`), не заворачивая её в `rule<T>`:

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                 .with(vd::statics::field(&Point::get_y, vd::double_bounds::greater_than(0.0)));
```

```cpp
// Проверка типов из тестов:
auto rule_obj    = vd::member(&Point::x, checker);
auto static_rule = vd::statics::member(&Point::x, checker);
static_assert(std::same_as<decltype(rule_obj), vd::rule<Point>>);
static_assert(!std::same_as<decltype(static_rule), vd::rule<Point>>);   // без std::function
```

Отдельного `vd::statics::predicate` нет — для правил уровня объекта без готового геттера/поля используйте сырую лямбду напрямую (см. ниже).

### 3. Сырые callable (лямбды, функторы) без фабрики

```cpp
auto model = vd::make_static_model<Point>()
                 .with([](const Point& p) { return p.x > 0 && p.y > 0; });
```

Все три стиля можно свободно комбинировать в одной модели — `.with()` требует только, чтобы очередной аргумент удовлетворял `static_rule_for<Rule, T>`.

---

## `check()` / `short_check()` / `die_if_failed()`

Контракт идентичен `basic_model`, но реализован через fold-выражения по кортежу вместо цикла по вектору:

- `check()` — comma-fold (`(..., invoke_one(rule))`): вызывает **все** правила безусловно, агрегирует все ошибки через `vd::result::with_other()`. Даже если первое правило провалилось, остальные всё равно выполняются (побочные эффекты внутри правил тоже произойдут).
- `short_check()` — `&&`-fold (`(... && check_one(rule))`): останавливается на первом провалившемся правиле, возвращает только его ошибку.
- `die_if_failed()` — вызывает `check()`, при провале бросает `vd::validation_exception` через `vd::require<vd::validation_exception>`.
- Все методы имеют перегрузку по указателю; `nullptr` даёт `result(false)` без вызова правил.

Поскольку каждое правило в кортеже имеет собственный статический тип, `check()`/`short_check()` определяют через `if constexpr` (по `std::invoke_result_t` конкретного правила), возвращает ли оно `vd::result` или `bool` — эта развилка решается индивидуально для каждого правила во время компиляции, а не унифицированно в рантайме, как у `basic_model` (там всё уже приведено к `rule<T>`, нормализующему возврат к `vd::result` внутри себя).

---

## Композиция моделей

`static_model` — обычное значение, его можно захватить по значению внутри лямбды и использовать как вложенное правило:

```cpp
auto address_model = vd::make_static_model<Address>()
                          .with(vd::member(&Address::city, vd::string_rules::non_empty()))
                          .with(vd::member(&Address::zip,  vd::string_rules::non_empty()));

auto contact_model = vd::make_static_model<Contact>()
                          .with([address_model](const Contact& c) {
                              return address_model.short_check(c.address);
                          });
```

---

## `validate_many`

Тот же набор перегрузок, что и для `basic_model` (см. [models.md](models.md#свободные-функции-validate_many)), только принимает `static_model<T, Rules...>`:

```cpp
bool ok = vd::validate_many(model, a, b, c);                 // variadic, одинаковый T
bool ok = vd::validate_many(model, std::vector<Point>{...}); // std::vector<T>
bool ok = vd::validate_many(model, std::vector<Point*>{...});// std::vector<T*> / std::vector<const T*>
```

---

## Ограничения

- **Нет `static_bound_model`.** У `basic_model` есть `basic_bound_model<T>` (через `.bind()`); у `static_model` эквивалента нет.
- **Нет `add_rule()`.** Мутация на месте архитектурно невозможна — набор правил зафиксирован в типе.
- **Нет `vd::statics::predicate`.** Только `field`/`member`; для произвольных предикатов над объектом передавайте лямбду в `.with()` напрямую.
- **`check()`/`short_check()` не помечены `constexpr`.** Конструкторы и `with()` — да, но фактическое выполнение проверки на этапе компиляции не задействовано (это не протестировано и не гарантируется текущей реализацией).

## Когда использовать что

- Набор правил известен целиком на этапе компиляции, важна минимизация аллокаций (`std::function`) — берите `static_model` + `vd::statics::field`/`vd::statics::member`.
- Правила формируются динамически, модель нужно хранить в контейнере разнородных объектов, передавать как значение с единым типом независимо от количества правил, или нужен `.bind()` — используйте `basic_model`.
