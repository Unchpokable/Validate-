#pragma once

#ifndef VD_RULE_FACTORY_HXX
#define VD_RULE_FACTORY_HXX

#include "vd_rule.hxx"

namespace vd
{

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
auto predicate(Fn fn) -> rule<typename detail::first_arg_of<Fn>::type>
{
    using T = typename detail::first_arg_of<Fn>::type;
    return rule<T>(std::move(fn));
}
} // namespace vd

#endif
