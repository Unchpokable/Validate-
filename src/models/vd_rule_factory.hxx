#pragma once

#ifndef VD_RULE_FACTORY_HXX
#define VD_RULE_FACTORY_HXX

#include <format>
#include <type_traits>

#include "core/vd_always_false.hxx"

#include "models/vd_rule.hxx"

namespace vd::detail
{
template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto field_impl(std::string_view field_name, MemberPtr ptr, Checker checker)
{
    using T = member_class_t<MemberPtr>;

    return [field_name = std::string(field_name), ptr, checker](const T& obj) -> vd::result {
        decltype(auto) val = std::invoke(ptr, obj);

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
    };
}

template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto member_impl(std::string_view member_name, MemberPtr ptr, Checker checker)
{
    using T = member_class_t<MemberPtr>;

    return [member_name = std::string(member_name), ptr, checker](const T& obj) -> vd::result {
        decltype(auto) val = std::invoke(ptr, obj);

        using checker_return = std::invoke_result_t<Checker, decltype(val)>;

        if constexpr(std::same_as<checker_return, vd::result>) {
            vd::result r = checker(val);
            if(!r) {
                return vd::result::failed({ std::format("Member {} failed: {}", member_name, r.short_format()) });
            }
            return vd::result::ok();
        }
        else if constexpr(std::convertible_to<checker_return, bool>) {
            bool check_passed = static_cast<bool>(checker(val));
            return check_passed ? vd::result::ok() : vd::result::failed({ std::format("Field '{}' failed validation", member_name) });
        }
        else {
            static_assert(vd::always_false_v<Checker>, "Checker must return bool or vd::result");
        }
    };
}
} // namespace vd::detail

namespace vd
{

// ---------------------------------------------------------------------------
// Factory functions

// vd::field — member function pointer (getter) + checker
// Example: vd::field("Foo::x", &Foo::get_x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto field(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;
    return rule<T>(detail::field_impl(field_name, ptr, checker));
}

template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto field(MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    return field("Unnamed", ptr, std::move(checker));
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto member(std::string_view field_name, MemberPtr ptr, Checker checker) -> rule<member_class_t<MemberPtr>>
{
    using T = member_class_t<MemberPtr>;
    return rule<T>(detail::member_impl(field_name, ptr, checker));
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
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

namespace vd::statics
{
// ---------------------------------------------------------------------------
// Factory functions for use with static_model, returning raw labmdas instead of vd::rule, since static_model designed to be heapalloc-free

// vd::field — member function pointer (getter) + checker
// Example: vd::field("Foo::x", &Foo::get_x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto field(std::string_view field_name, MemberPtr ptr, Checker checker)
{
    return detail::field_impl(field_name, ptr, checker);
}

template<typename MemberPtr, typename Checker>
requires std::is_member_function_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto field(MemberPtr ptr, Checker checker)
{
    return vd::statics::field("Unnamed", ptr, std::move(checker));
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto member(std::string_view field_name, MemberPtr ptr, Checker checker)
{
    return detail::member_impl(field_name, ptr, checker);
}

// vd::member — member variable pointer + checker
// Example: vd::member(&Foo::x, some_bounds)
template<typename MemberPtr, typename Checker>
requires std::is_member_object_pointer_v<std::remove_cvref_t<MemberPtr>>
         && value_checker<Checker, std::invoke_result_t<MemberPtr, const member_class_t<MemberPtr>&>>
auto member(MemberPtr ptr, Checker checker)
{
    return vd::statics::member("Unnamed", ptr, std::move(checker));
}
} // namespace vd::statics

#endif
