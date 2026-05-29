#pragma once

#ifndef VD_RULE_FACTORY_HXX
#define VD_RULE_FACTORY_HXX

#include <format>
#include <type_traits>

#include "core/vd_always_false.hxx"

#include "models/vd_rule.hxx"

namespace vd
{

// ---------------------------------------------------------------------------
// Factory functions

// vd::field — member function pointer (getter) + checker
// Example: vd::field("Foo::x", &Foo::get_x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;
    return rule<T>([field_name, ptr, checker](const T& obj) -> vd::result {
        auto val = std::invoke(ptr, obj);

        using checker_return = std::invoke_result_t<Checker, decltype(val)>;

        if constexpr(std::same_as<checker_return, vd::result>) {
            vd::result r = checker(val);
            if(!r) {
                return vd::result::failed({ std::format("Field {} failed: {}", field_name, r.short_format()) });
            }
            return vd::result::ok();
        }
        else if constexpr(std::convertible_to<checker_return, bool>) {
            bool check_passed = static_cast<bool>(checker(val));
            return check_passed ? vd::result::ok() : vd::result::failed({ std::format("Field '{}' failed validation", field_name) });
        }
        else {
            static_assert(vd::always_false_v<Checker>, "Checker must return bool or vd::result");
        }
    });
}

template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
auto field(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    return field("Unnamed", ptr, std::move(checker));
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;

    return rule<T>([field_name, ptr, checker](const T& obj) -> vd::result {
        auto val = std::invoke(ptr, obj);

        using checker_return = std::invoke_result_t<Checker, decltype(val)>;

        if constexpr(std::same_as<checker_return, vd::result>) {
            vd::result r = checker(val);
            if(!r) {
                return vd::result::failed({ std::format("Field {} failed: {}", field_name, r.short_format()) });
            }
            return vd::result::ok();
        }
        else if constexpr(std::convertible_to<checker_return, bool>) {
            bool check_passed = static_cast<bool>(checker(val));
            return check_passed ? vd::result::ok() : vd::result::failed({ std::format("Field '{}' failed validation", field_name) });
        }
        else {
            static_assert(vd::always_false_v<Checker>, "Checker must return bool or vd::result");
        }
    });
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
auto member(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    return member("Unnamed", ptr, std::move(checker));
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

namespace vd
{

} // namespace vd

#endif
