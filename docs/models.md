# Models module

**Header:** `#include <vd.hxx>`  
**Implementation files:** `src/models/vd_rule.hxx`, `src/models/vd_basic_model.hxx`  
**Namespace:** `vd`

---

## Core idea

The module is built on three levels:

```
rule<T>              — a single rule: const T& -> vd::result
basic_model<T>       — a set of rules + check() / short_check() methods
basic_bound_model<T> — a model bound to a specific object
```

A rule (`rule<T>`) is a type-erased wrapper around a predicate. All the ways of defining a rule (via a member pointer, via a getter, via a lambda) are reduced to a single internal `std::function<vd::result(const T&)>`.

**Compile-time alternative.** If the full set of rules is known at compile time and avoiding `std::function` allocations matters, there's `vd::static_model<T, Rules...>` — a model whose rule set is part of the type (`std::tuple<Rules...>`), not the value. See [static-model.md](static-model.md).

---

## `vd::rule<T>`

### Constructor

```cpp
template<typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, rule>)
         && std::invocable<Fn, const T&>
         && std::convertible_to<std::invoke_result_t<Fn, const T&>, bool>
explicit rule(Fn&& fn);
```

Accepts any callable with the signature `const T& -> bool` or `const T& -> vd::result` (since `vd::result` converts to `bool`). The constructor is `explicit` — rules are created through factory functions, not implicit conversion.

```cpp
// Direct creation (rarely needed manually)
vd::rule<int> r([](const int& v) { return v > 0; });
```

### `operator()`

```cpp
vd::result operator()(const T& obj) const;
vd::result operator()(const T* obj) const;  // dereferences and calls the reference version
```

Both overloads exist so that `basic_model::check(const T*)` can invoke the rule through a pointer without extra code.

---

## Factory functions

This is the primary way to create rules. `T` is deduced automatically from the member pointer type.

### `vd::field` — getter method + checker

```cpp
// With a field name (used in error messages):
template<typename MemberPtr, typename Checker>
    requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;

// Without a name:
template<typename MemberPtr, typename Checker>
    requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;
```

Used when the value to check needs to be obtained by calling a method on the object.

```cpp
struct Sensor {
    double get_temperature() const;
};

auto rule = vd::field("temperature", &Sensor::get_temperature, vd::double_bounds::less_than(100.0));
// T = Sensor is deduced from the type of &Sensor::get_temperature
```

**Method requirement:** must be `const` — the object is passed as `const T&`.

### `vd::member` — field + checker

```cpp
// With a field name (used in error messages):
template<typename MemberPtr, typename Checker>
    requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;

// Without a name:
template<typename MemberPtr, typename Checker>
    requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>;
```

Used for direct access to an object's field.

```cpp
struct Point { double x, y; };

auto rule = vd::member("x", &Point::x, vd::double_bounds::inclusive(0.0, 100.0));
// T = Point is deduced from the type of &Point::x
```

`vd::field` and `vd::member` are intentionally split by name, even though the implementation is identical. The name difference reflects intent: *computed value* vs *stored field*. Providing `field_name` adds context to the error message when the rule fails.

### `vd::predicate` — arbitrary lambda

```cpp
template<typename Fn>
auto predicate(Fn fn) -> rule</*T is deduced from Fn's first argument*/>;
```

`T` is deduced from the lambda's signature via `detail::first_arg_of`. Works for non-generic, non-mutable lambdas (the most common case).

```cpp
auto rule = vd::predicate([](const Point& p) {
    return p.x >= 0 && p.y >= 0 && p.x * p.x + p.y * p.y < 10000.0;
});
```

For generic lambdas, or cases where deducing `T` isn't possible — create `rule<T>` directly:

```cpp
vd::rule<MyType> r([](const MyType& v) { return v.is_valid_state(); });
```

---

## `vd::basic_model<T>`

Stores `std::vector<rule<T>>` and applies all rules sequentially.

### Building a model

```cpp
// Empty model (accepts any object)
vd::basic_model<T> model;

// Via an initializer_list in the constructor
vd::basic_model<T> model({ rule1, rule2, rule3 });

// Via the fluent builder
auto model = vd::basic_model<T>()
    .with(rule1)
    .with(rule2);

// Adding a set of rules
model.with({ rule3, rule4 });

// Merging with another model
auto combined = vd::basic_model<T>().with(base_model).with(extra_rule);
```

### `with()` — ref-qualified overloads

`with` has two variants depending on the value category of the model object:

```cpp
basic_model& with(rule<T>) &;     // lvalue: mutates in place, returns *this
basic_model  with(rule<T>) &&;    // rvalue: adds the rule and moves the model
```

**Why this matters.** The pattern `auto model = vd::basic_model<T>().with(...)` goes through the rvalue version: the temporary object is moved into `model`, without copying the vector of rules. If `with` always returned `basic_model&`, then `auto model = ...` would require a copy — problematic on MSVC for certain sizes of data captured in `std::function`.

```cpp
// rvalue path — move, not copy:
auto model = vd::basic_model<T>().with(rule1).with(rule2);

// lvalue path — in-place modification:
vd::basic_model<T> model;
model.with(rule1).with(rule2);  // returns basic_model&
```

### `check()`

```cpp
vd::result check(const T& object) const;
vd::result check(const T* object) const;
```

Runs **all** rules in sequence and aggregates results via `result::with_other()`. Does not stop at the first failure — collects messages from every failing rule. An empty model returns `result::ok()`.

If `object == nullptr`, returns `result(false)` without running any rules.

### `short_check()`

```cpp
vd::result short_check(const T& object) const;
vd::result short_check(const T* object) const;
```

Fail-fast variant of `check()`. Stops at the first failing rule and returns immediately. Used when it's enough to know that an error exists, without a full report — e.g. row-by-row form validation or a hot path.

If `object == nullptr`, returns `result(false)` without running any rules.

### `die_if_failed()`

```cpp
void die_if_failed(const T& object) const;
void die_if_failed(const T* object) const;
```

Calls `check()` and, if the result is not valid, throws `vd::validation_exception`.

### `add_rule()`

```cpp
void add_rule(rule<T> rule);
```

Adds a rule without returning a reference. Used when a fluent chain isn't needed.

---

## `vd::basic_bound_model<T>`

Binds a model to a specific object. Convenient when the same (model, object) pair needs to be checked multiple times or passed around.

```cpp
vd::basic_model<Point> model = /* ... */;
Point p{3.0, 4.0};

auto bound = model.bind(p);
vd::result r = bound.check();        // equivalent to model.check(p)
vd::result r = bound.short_check();  // equivalent to model.short_check(p)
```

### `check()` / `short_check()` / `die_if_failed()`

```cpp
vd::result check() const;
vd::result short_check() const;
void die_if_failed() const;
```

Delegate to `m_model.check(m_object)`, `m_model.short_check(m_object)`, and `m_model.die_if_failed(m_object)` respectively.

### Important limitation: non-owning reference

`basic_bound_model<T>` holds a **non-owning reference** to the model (`const basic_model<T>&`). The model must outlive the `basic_bound_model` instance.

```cpp
// Correct: model outlives bound
auto model = vd::basic_model<Point>().with(...);
auto bound = model.bind(point);
bound.check();  // OK

// Incorrect: dangling reference
auto bound = vd::basic_model<Point>().with(...).bind(point); // Impossible, the rvalue-qualified bind() overload is deleted
```

If independence from the source model's lifetime is needed — call `model.check(point)` directly.

### `bind()` and nullptr

Passing `nullptr` to `bind` triggers a contract violation of `basic_bound_model<T>::bind(vd::not_null<const_pointer_type>)` — either a compile error when passing nullptr directly, or a call to `std::terminate` at runtime.

```cpp
model.bind(nullptr);  // compile error
```

---

## Helper traits

### `member_class_t<Ptr>`

Extracts the class type from any member pointer (field or method):

```cpp
member_class_t<double Point::*>             // -> Point
member_class_t<double(Point::*)() const>    // -> Point
```

Works through a single specialization `V T::*` — the pattern matches both data pointers (`double Point::*`) and method pointers (since `double(Point::*)() const` also reduces to the form `V T::*`, where `V = double() const`).

### `detail::first_arg_of<Fn>`

A recursive trait that extracts the type of the first argument from a callable's `operator() const`. Used only inside `vd::predicate`. Works for non-generic, non-mutable lambdas. Mutable and generic lambdas are not supported (no `const`-qualified `operator()`).

### `value_checker<Checker, V>` concept

```cpp
template<typename Checker, typename V>
concept value_checker = std::invocable<Checker, V>
    && std::convertible_to<std::invoke_result_t<Checker, V>, bool>;
```

Used as concept documentation. Any type satisfying this concept can be passed as the second argument to `vd::field` / `vd::member`. Since `vd::result` has `operator bool()`, checkers that return `vd::result` also satisfy this concept.

---

## Free functions `validate_many`

Helper functions for validating several objects at once. Return `bool` (not `vd::result`).

```cpp
// An arbitrary number of objects of the same type (variadic):
template<typename T, typename... Args>
    requires(std::same_as<std::decay_t<Args>, T> && ...)
bool validate_many(const basic_model<T>& model, Args&&... objects);

// A vector of objects by value:
template<typename T>
bool validate_many(const basic_model<T>& model, const std::vector<T>& objects);

// A vector of pointers (non-const):
template<typename T>
    requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<T*>& object_ptrs);

// A vector of pointers (const):
template<typename T>
    requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<const T*>& object_ptrs);
```

Returns `true` only if all objects passed validation.
