#pragma once

#ifndef VD_RULE_HXX
#define VD_RULE_HXX

#include <concepts>
#include <functional>
#include <type_traits>

namespace vd
{

// Extracts the class type from any member pointer (variable or function).
// member_class_t<double example::*>                  -> example
// member_class_t<double(example::*)() const>         -> example
template<typename>
struct member_class;

template<typename T, typename V>
struct member_class<V T::*> {
    using type = T;
};

template<typename Ptr>
using member_class_t = typename member_class<std::remove_cvref_t<Ptr>>::type;

namespace details
{
// Extracts the first argument type from a non-generic callable (e.g. non-mutable lambda).
template<typename Fn>
struct first_arg_of : first_arg_of<decltype(&std::remove_cvref_t<Fn>::operator())> {};

template<typename C, typename R, typename Arg, typename... Rest>
struct first_arg_of<R (C::*)(Arg, Rest...) const> {
    using type = std::remove_cvref_t<Arg>;
};

template<typename C, typename R, typename Arg, typename... Rest>
struct first_arg_of<R (C::*)(Arg, Rest...) const noexcept> {
    using type = std::remove_cvref_t<Arg>;
};
} // namespace details

// Checker concept: any callable V -> bool-convertible.
template<typename Checker, typename V>
concept value_checker = std::invocable<Checker, V>
    && std::convertible_to<std::invoke_result_t<Checker, V>, bool>;

// ---------------------------------------------------------------------------

template<typename T>
struct rule {
    template<typename Fn>
    requires std::invocable<Fn, const T&>
          && std::convertible_to<std::invoke_result_t<Fn, const T&>, bool>
    explicit rule(Fn&& fn) : m_predicate(std::forward<Fn>(fn))
    {}

    bool operator()(const T& obj) const { return m_predicate(obj); }
    bool operator()(const T* obj) const { return m_predicate(*obj); }

private:
    std::function<bool(const T&)> m_predicate;
};

// ---------------------------------------------------------------------------
// Factory functions

// vd::field — member function pointer (getter) + checker
// Example: vd::field(&Foo::get_x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;
    return rule<T>([ptr, checker](const T& obj) -> bool {
        return static_cast<bool>(checker(std::invoke(ptr, obj)));
    });
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;
    return rule<T>([ptr, checker](const T& obj) -> bool {
        return static_cast<bool>(checker(std::invoke(ptr, obj)));
    });
}

// vd::predicate — black-box, T deduced from the lambda's first argument.
// For generic lambdas or std::function, construct rule<T> directly.
// Example: vd::predicate([](const Foo& f) { return f.x > 0; })
template<typename Fn>
auto predicate(Fn fn) -> rule<typename details::first_arg_of<Fn>::type>
{
    using T = typename details::first_arg_of<Fn>::type;
    return rule<T>(std::move(fn));
}

} // namespace vd

#endif // VD_RULE_HXX
