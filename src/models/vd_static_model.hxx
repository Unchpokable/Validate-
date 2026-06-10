#pragma once

#ifndef VD_STATIC_MODEL_HXX
#define VD_STATIC_MODEL_HXX

#include <algorithm>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "assert/vd_assert.hxx"

#include "core/vd_exception.hxx"
#include "core/vd_result.hxx"

namespace vd
{
// Rule concept: callable with const T& → vd::result or bool-convertible.
// Distinct from value_checker (which is for field values); this operates on the validated object itself.
template<typename Rule, typename T>
concept static_rule_for = std::invocable<Rule, const T&>
                          && (std::convertible_to<std::invoke_result_t<Rule, const T&>, vd::result>
                              || std::convertible_to<std::invoke_result_t<Rule, const T&>, bool>);

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>) && (static_rule_for<Rules, T> && ...)
struct static_model final {
    using value_type = T;
    using const_reference_type = const T&;
    using const_pointer_type = const T*;

    constexpr static_model()
    requires(sizeof...(Rules) == 0)
    = default;

    constexpr explicit static_model(Rules... rules)
    requires(sizeof...(Rules) > 0)
        : m_rules(std::move(rules)...)
    {
    }

    [[nodiscard]] vd::result check(const_reference_type object) const;

    [[nodiscard]] vd::result check(const_pointer_type object) const
    {
        if(object == nullptr) {
            return vd::result(false);
        }
        return check(*object);
    }

    [[nodiscard]] vd::result short_check(const_reference_type object) const;

    [[nodiscard]] vd::result short_check(const_pointer_type object) const
    {
        if(object == nullptr) {
            return vd::result(false);
        }
        return short_check(*object);
    }

    void die_if_failed(const_reference_type object) const
    {
        vd::require<vd::validation_exception>(check(object), "Object failed validation rules");
    }

    void die_if_failed(const_pointer_type object) const
    {
        vd::require<vd::validation_exception>(check(object), "Object failed validation rules");
    }

    // Returns a new static_model<T, Rules..., NewRule> — does not mutate this.
    template<typename NewRule>
    requires static_rule_for<NewRule, T>
    [[nodiscard]] constexpr auto with(NewRule new_rule) const& -> static_model<T, Rules..., NewRule>
    {
        return std::apply(
            [&](const auto&... existing) {
                return static_model<T, Rules..., NewRule>(existing..., std::move(new_rule));
            },
            m_rules);
    }

    template<typename NewRule>
    requires static_rule_for<NewRule, T>
    [[nodiscard]] constexpr auto with(NewRule new_rule) && -> static_model<T, Rules..., NewRule>
    {
        return std::apply(
            [&](auto&&... existing) {
                return static_model<T, Rules..., NewRule>(std::forward<decltype(existing)>(existing)..., std::move(new_rule));
            },
            std::move(m_rules));
    }

private:
    std::tuple<Rules...> m_rules;
};

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>) && (static_rule_for<Rules, T> && ...)
vd::result static_model<T, Rules...>::check(const_reference_type object) const
{
    vd::result final_result = vd::result::ok();
    std::apply(
        [&](const auto&... rule) {
            auto invoke_one = [&](const auto& r) {
                using R = std::invoke_result_t<decltype(r), const T&>;
                if constexpr(std::same_as<std::remove_cvref_t<R>, vd::result>) {
                    final_result.with_other(r(object));
                }
                else {
                    if(!static_cast<bool>(r(object))) {
                        final_result.with_other(vd::result::failed({ "Rule failed" }));
                    }
                }
            };
            (..., invoke_one(rule));
        },
        m_rules);
    return final_result;
}

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>) && (static_rule_for<Rules, T> && ...)
vd::result static_model<T, Rules...>::short_check(const_reference_type object) const
{
    vd::result last = vd::result::ok();
    std::apply(
        [&](const auto&... rules) {
            auto check_one = [&](const auto& r) -> bool {
                using R = std::invoke_result_t<decltype(r), const T&>;
                if constexpr(std::same_as<std::remove_cvref_t<R>, vd::result>) {
                    last = r(object);
                    return last.is_valid;
                }
                else {
                    bool ok = static_cast<bool>(r(object));
                    if(!ok) {
                        last = vd::result::failed({ "Rule failed" });
                    }
                    return ok;
                }
            };
            (... && check_one(rules));
        },
        m_rules);
    return last;
}
} // namespace vd

namespace vd
{
template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
constexpr auto make_static_model() -> static_model<T>
{
    return {};
}
} // namespace vd

namespace vd
{
template<typename T, typename... Rules, typename... Args>
requires(std::same_as<std::decay_t<Args>, T> && ...)
bool validate_many(const static_model<T, Rules...>& model, Args&&... objects)
{
    return (model.short_check(objects).is_valid && ...);
}

template<typename T, typename... Rules>
bool validate_many(const static_model<T, Rules...>& model, const std::vector<T>& objects)
{
    return std::ranges::all_of(objects, [&](const T& obj) {
        return model.short_check(obj).is_valid;
    });
}

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const static_model<T, Rules...>& model, const std::vector<T*>& object_ptrs)
{
    return std::ranges::all_of(object_ptrs, [&](const T* obj) {
        return model.short_check(obj).is_valid;
    });
}

template<typename T, typename... Rules>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const static_model<T, Rules...>& model, const std::vector<const T*>& object_ptrs)
{
    return std::ranges::all_of(object_ptrs, [&](const T* obj) {
        return model.short_check(obj).is_valid;
    });
}
} // namespace vd

#endif // VD_STATIC_MODEL_HXX
