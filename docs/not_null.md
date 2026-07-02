# not_null module

**Header:** `#include <vd.hxx>` (transitively via `vd_models.hxx`)  
**Implementation file:** `src/core/vd_not_null.hxx`  
**Namespace:** `vd`

---

## Purpose

`vd::not_null<T*>` is a wrapper around a raw pointer that expresses the precondition "the pointer is never `nullptr`" directly in the function signature. The principle: if the rule is violated, the error is caught at the point the argument is passed, not somewhere inside the function.

```cpp
// Without not_null — the violation is caught (at best) at dereference time inside
void process(Widget* w) {
    w->do_something();  // UB if w == nullptr
}

// With not_null — the violation is caught at the call site
void process(vd::not_null<Widget*> w) {
    w->do_something();  // guaranteed safe
}
```

---

## Definition

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

Only accepts pointer types (`T*`, `const T*`). `not_null<int>` does not compile.

---

## Construction

### From nullptr — forbidden

```cpp
constexpr not_null(std::nullptr_t) = delete;
```

Passing `nullptr` directly does not compile:

```cpp
vd::not_null<int*> p = nullptr;  // compile error
```

### From a non-null pointer

```cpp
constexpr not_null(pointer_type ptr);
```

In a `constexpr` context, checks `ptr != nullptr` statically — a violation is a compile error. In a runtime context, `ptr == nullptr` calls `std::terminate()` with a diagnostic message on `stderr`:

```
not_null cannot be constructed with nullptr
```

### Covariant copying

```cpp
template<typename U>
requires std::is_convertible_v<U, T>
constexpr not_null(const not_null<U>& other) noexcept;
```

Allows implicitly converting `not_null<Derived*>` to `not_null<Base*>`.

---

## Operators

```cpp
constexpr reference_type operator*()  const noexcept;  // dereference
constexpr pointer_type   operator->() const noexcept;  // member access
constexpr operator pointer_type()     const noexcept;  // implicit conversion to T*

constexpr pointer_type get() const noexcept;           // explicit pointer retrieval

friend constexpr auto operator<=>(not_null, not_null) = default;
friend constexpr bool operator==(not_null, not_null)  = default;
```

`not_null<T*>` transparently converts to `T*`, so it can be passed to functions expecting a raw pointer without an explicit `get()` call.

---

## Ownership

`not_null<T*>` **does not own** the object. It's a non-null observer, analogous to a raw pointer in ownership semantics. For transferring ownership, use `std::unique_ptr` or `std::shared_ptr`; `not_null` is meant for pointers borrowed from the calling code.

---

## Typical patterns

### Function parameter

```cpp
void render(vd::not_null<Renderer*> r, vd::not_null<const Scene*> scene) {
    r->draw(*scene);
}
```

### Class field

```cpp
class View {
    vd::not_null<Model*> m_model;
public:
    explicit View(vd::not_null<Model*> model) : m_model(model) {}
};
```

### Compatibility with basic_model

Inside the library, `not_null` is used in `basic_model::bind()` and `basic_bound_model::bind()` — these methods accept `not_null<const T*>`, explicitly documenting that `nullptr` is not a valid argument:

```cpp
auto bound = model.bind(vd::not_null<const Point*>(&point));
```

When a raw non-null pointer is passed, the compiler builds `not_null` implicitly.

---

## Difference from `vd::memory::not_null`

Not to be confused with the identically-named but very different `vd::memory::not_null` (`src/models/vd_memory.hxx`, `Namespace: vd::memory`). This is not a wrapper type, but a **ready-made checker** — a value invocable as a `value_checker`, intended for use inside `vd::member`/`vd::field`:

```cpp
inline constexpr detail::not_null_t not_null;   // vd::memory::not_null

vd::result operator()(const T& ptr) const;      // T must be pointer-like (raw pointer, shared_ptr, unique_ptr, QPointer, …)
```

`vd::memory::not_null` checks a model object's field for `!= nullptr` and returns `vd::result` (no abort/terminate — just a rule failure with the message `"Pointer must not be null"`), whereas `vd::not_null<T*>` (described earlier in this document) is a function-parameter/class-field type that guarantees non-nullness at the contract level (terminate/compile error on construction). Example of using `vd::memory::not_null` inside a model:

```cpp
struct Node {
    Node* parent;
    std::shared_ptr<Data> data;
};

auto model = vd::basic_model<Node>()
    .with(vd::member(&Node::parent, vd::memory::not_null))
    .with(vd::member(&Node::data,   vd::memory::not_null));
```

`pointer_like<T>` is a concept requiring only `{ ptr == nullptr } -> convertible_to<bool>`; types that don't satisfy it produce a `static_assert` with a clear message instead of an obscure template error.

---

## Comparison with gsl::not_null

`vd::not_null<T>` is intentionally minimal. Unlike `gsl::not_null`:

- no smart pointer support (`unique_ptr`, `shared_ptr`)
- no `[[gsl::Owner]]` / `[[gsl::Pointer]]` annotations
- no dependency on GSL

The library is aimed at scenarios of passing borrowed raw pointers in a validation context.
