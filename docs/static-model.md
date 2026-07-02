# Static model module

**Header:** `#include <vd.hxx>`
**Implementation files:** `src/models/vd_static_model.hxx`, factories `vd::statics::field` / `vd::statics::member` in `src/models/vd_rule_factory.hxx`
**Namespace:** `vd`, `vd::statics`

---

## Purpose

`vd::static_model<T, Rules...>` is the second model type in the library, alongside `vd::basic_model<T>` (see [models.md](models.md)). The idea is the same: a set of rules + `check()`/`short_check()`. The difference is where that set lives.

`basic_model<T>` stores rules in a `std::vector<rule<T>>`: each rule is a `rule<T>`, a wrapper around `std::function<vd::result(const T&)>`. This is convenient (rules can be added at runtime, models can be stored in containers, etc.), but every rule created via `vd::field`/`vd::member`/`vd::predicate` almost certainly means a heap allocation for the `std::function`.

`static_model<T, Rules...>` stores rules in a `std::tuple<Rules...>` — the set of rules is part of the model's **type**, not its value. If the rule's type is a capturing lambda or a stateless simple functor, the compiler can unroll the check entirely without a single heap allocation. This is the case when you need a fully static, heap-alloc-free validation pipeline (e.g. on a hot path or in an embedded context), and the full set of rules is known at compile time.

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 100.0)))
                 .with(vd::statics::member(&Point::y, vd::double_bounds::inclusive(0.0, 100.0)))
                 .with([](const Point& p) { return p.x != p.y; });

vd::result r = model.check(point);
```

---

## Comparison with `basic_model<T>`

| Aspect | `basic_model<T>` | `static_model<T, Rules...>` |
|---|---|---|
| Rule storage | `std::vector<rule<T>>` (runtime, type-erased via `std::function`) | `std::tuple<Rules...>` (compile-time, each rule is a separate type) |
| The rule set is part of | the value (can be changed at runtime) | the **type** (`.with()` changes the model's type) |
| `with()` on an lvalue | mutates the object in place, returns `basic_model&` | no in-place mutation at all — only `const&`/`&&` overloads, both returning a **new** `static_model<T, Rules..., NewRule>` |
| `add_rule()` | present | absent (impossible in principle — the rule set is fixed in the type) |
| Heap allocations | almost always (every rule is a `std::function`) | not required, if you use `vd::statics::field`/`vd::statics::member`/raw lambdas |
| `constexpr` | no | constructors and `with()` are `constexpr`; `check()`/`short_check()` are not (see "Limitations" below) |
| Bound model (`.bind()`) | `basic_bound_model<T>` | absent — no equivalent |

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
constexpr auto make_static_model() -> static_model<T>;   // empty static_model<T>
```

### `static_rule_for<Rule, T>` vs `value_checker<Checker, V>` — why two similar concepts

Both concepts are structurally identical ("callable → bool or vd::result"), but they describe different roles in the pipeline:

- `value_checker<Checker, V>` (in `vd_rule.hxx`) validates a **field's value**: `V -> bool | vd::result`. This is what's passed as the second argument to `vd::field`/`vd::member` (e.g. `vd::double_bounds::inclusive(...)`, `vd::string_rules::non_empty()`).
- `static_rule_for<Rule, T>` validates the **whole object**: `const T& -> bool | vd::result`. This is what `static_model::with()` accepts.

The separation keeps the two responsibilities type-safely distinguishable: a field checker can't be confused with an object-level rule, even though both structurally look like "something callable returning a bool". `basic_model` doesn't need a separate concept of the same kind — there, the rule's role is already fixed by the `rule<T>` type itself.

### Empty model and `nullptr`

A model with no rules (`sizeof...(Rules) == 0`, created via `make_static_model<T>()`) is always valid for any object — but not for `nullptr`: `check(nullptr)`/`short_check(nullptr)` return `result(false)` without consulting any rules, just like `basic_model`.

### `with()` immutability

Each call to `.with(rule)` does not modify the existing model — it **creates a new** model of type `static_model<T, Rules..., NewRule>`. The original object (if called via `const&`) remains unchanged and valid:

```cpp
auto base = vd::make_static_model<Point>()
                .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)));

auto extended = base.with(vd::member(&Point::y, vd::double_bounds::inclusive(0.0, 10.0)));

// base checks only x, extended checks x and y. base is unchanged.
```

This is a direct consequence of the rule set being part of the type: `basic_model<T>` has the same type regardless of the number of rules, so in-place mutation is possible and natural; for `static_model<T, Rules...>`, adding a rule changes the object's type itself, so "in-place mutation" is syntactically impossible — instead, `with()` always returns a new value (of a new type).

---

## Building rules

`static_model` accepts any callable satisfying `static_rule_for<Rule, T>` — the representation of a rule isn't fixed, and the three styles can be freely mixed within a single model.

### 1. Regular `vd::rule<T>` (via `vd::field` / `vd::member` / `vd::predicate`)

The same factories used for `basic_model` work here too — but each such rule is still wrapped in `rule<T>`/`std::function`, so the heap allocation is not eliminated:

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                 .with(vd::predicate([](const Point& p) { return p.x != p.y; }));
```

### 2. `vd::statics::field` / `vd::statics::member` — heap-alloc-free factories

These use the same internal implementation (`detail::field_impl` / `detail::member_impl`) as `vd::field`/`vd::member`, but return the "raw" lambda directly (via `auto`) without wrapping it in `rule<T>`:

```cpp
auto model = vd::make_static_model<Point>()
                 .with(vd::statics::member(&Point::x, vd::double_bounds::inclusive(0.0, 10.0)))
                 .with(vd::statics::field(&Point::get_y, vd::double_bounds::greater_than(0.0)));
```

```cpp
// Type check from the tests:
auto rule_obj    = vd::member(&Point::x, checker);
auto static_rule = vd::statics::member(&Point::x, checker);
static_assert(std::same_as<decltype(rule_obj), vd::rule<Point>>);
static_assert(!std::same_as<decltype(static_rule), vd::rule<Point>>);   // no std::function
```

There is no separate `vd::statics::predicate` — for object-level rules without a ready-made getter/field, use a raw lambda directly (see below).

### 3. Raw callables (lambdas, functors) without a factory

```cpp
auto model = vd::make_static_model<Point>()
                 .with([](const Point& p) { return p.x > 0 && p.y > 0; });
```

All three styles can be freely combined within a single model — `.with()` only requires that the next argument satisfy `static_rule_for<Rule, T>`.

---

## `check()` / `short_check()` / `die_if_failed()`

The contract is identical to `basic_model`, but implemented via fold expressions over the tuple instead of a loop over the vector:

- `check()` — comma-fold (`(..., invoke_one(rule))`): calls **all** rules unconditionally, aggregating all errors via `vd::result::with_other()`. Even if the first rule fails, the rest still run (side effects inside rules happen too).
- `short_check()` — `&&`-fold (`(... && check_one(rule))`): stops at the first failing rule, returning only its error.
- `die_if_failed()` — calls `check()`, throwing `vd::validation_exception` via `vd::require<vd::validation_exception>` on failure.
- All methods have a pointer overload; `nullptr` yields `result(false)` without invoking any rules.

Since each rule in the tuple has its own static type, `check()`/`short_check()` determine via `if constexpr` (based on `std::invoke_result_t` for that specific rule) whether it returns `vd::result` or `bool` — this branching is resolved individually per rule at compile time, rather than uniformly at runtime as in `basic_model` (there, everything is already normalized to `rule<T>`, which normalizes the return to `vd::result` internally).

---

## Composing models

`static_model` is an ordinary value — it can be captured by value inside a lambda and used as a nested rule:

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

The same set of overloads as for `basic_model` (see [models.md](models.md#free-functions-validate_many)), just accepting `static_model<T, Rules...>`:

```cpp
bool ok = vd::validate_many(model, a, b, c);                 // variadic, same T
bool ok = vd::validate_many(model, std::vector<Point>{...}); // std::vector<T>
bool ok = vd::validate_many(model, std::vector<Point*>{...});// std::vector<T*> / std::vector<const T*>
```

---

## Limitations

- **No `static_bound_model`.** `basic_model` has `basic_bound_model<T>` (via `.bind()`); `static_model` has no equivalent.
- **No `add_rule()`.** In-place mutation is architecturally impossible — the rule set is fixed in the type.
- **No `vd::statics::predicate`.** Only `field`/`member`; for arbitrary object-level predicates, pass a lambda directly to `.with()`.
- **`check()`/`short_check()` are not marked `constexpr`.** The constructors and `with()` are, but actual compile-time execution of the check is not exercised (it's neither tested nor guaranteed by the current implementation).

## When to use which

- The rule set is known entirely at compile time and minimizing allocations (`std::function`) matters — use `static_model` + `vd::statics::field`/`vd::statics::member`.
- Rules are formed dynamically, the model needs to be stored in a container of heterogeneous objects, passed as a value with a single type regardless of the rule count, or `.bind()` is needed — use `basic_model`.
