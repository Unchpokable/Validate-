#pragma once

#ifndef VD_RULE_HXX
#define VD_RULE_HXX

#include <concepts>
#include <functional>
#include <type_traits>

#include "core/vd_not_null.hxx"
#include "core/vd_result.hxx"

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

namespace detail
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
} // namespace detail

// Checker concept: any callable V -> bool-convertible.
template<typename Checker, typename V>
concept value_checker =
    std::invocable<Checker, V>
    && (std::convertible_to<std::invoke_result_t<Checker, V>, vd::result> || std::convertible_to<std::invoke_result_t<Checker, V>, bool>);

// ---------------------------------------------------------------------------

template<typename T>
struct rule {
    using rule_fn = std::function<vd::result(const T&)>;

    using const_pointer_type = std::add_pointer_t<std::add_const_t<T>>;
    using const_reference_type = std::add_lvalue_reference_t<std::add_const_t<T>>;

    template<typename Fn>
    requires(!std::same_as<std::remove_cvref_t<Fn>, rule>)
            && std::invocable<Fn, const_reference_type> && std::convertible_to<std::invoke_result_t<Fn, const_reference_type>, vd::result>
    explicit rule(Fn&& fn) : m_predicate(std::forward<Fn>(fn))
    {
    }

    rule(const rule&) = default;
    rule(rule&&) = default;
    rule& operator=(const rule&) = default;
    rule& operator=(rule&&) = default;

    vd::result operator()(const_reference_type obj) const
    {
        return m_predicate(obj);
    }

    vd::result operator()(vd::not_null<const_pointer_type> obj) const
    {
        return m_predicate(*obj);
    }

private:
    rule_fn m_predicate;
};
} // namespace vd

#endif // VD_RULE_HXX
