#pragma once

#include <concepts>
#ifndef VD_BASIC_MODEL_HXX
#define VD_BASIC_MODEL_HXX

#include <algorithm>
#include <type_traits>
#include <vector>

#include "models/vd_rule.hxx"

#include "assert/vd_assert.hxx"

#include "core/vd_exception.hxx"
#include "core/vd_result.hxx"

namespace vd
{
template<typename T>
struct basic_bound_model;

template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
struct basic_model {
    using const_pointer_type = std::add_pointer_t<std::add_const_t<T>>;
    using const_reference_type = std::add_lvalue_reference_t<std::add_const_t<T>>;

    basic_model() = default;
    basic_model(std::initializer_list<vd::rule<T>> rules) : m_rules(rules)
    {
    }

    vd::result is_valid(const_pointer_type object) const;

    vd::result is_valid(const_reference_type object) const
    {
        return is_valid(std::addressof(object));
    }

    void die_if_failed(const_pointer_type object) const
    {
        vd::require<vd::validation_exception>(is_valid(object), "Object failed validation rules");
    }

    void die_if_failed(const_reference_type object) const
    {
        die_if_failed(std::addressof(object));
    }

    basic_bound_model<T> bind(const_pointer_type object) const&
    {
        vd::require(object != nullptr, "Binding to nullptr objects is not allowed!");

        return basic_bound_model<T>::bind(*this, object);
    }

    basic_bound_model<T> bind(const_reference_type object) const&
    {
        return bind(std::addressof(object));
    }

    basic_bound_model<T> bind(const_pointer_type) const&& = delete;
    basic_bound_model<T> bind(const_reference_type) const&& = delete;

    void add_rule(vd::rule<T> rule)
    {
        m_rules.push_back(std::move(rule));
    }

    basic_model& with(vd::rule<T> rule) &
    {
        add_rule(std::move(rule));
        return *this;
    }

    basic_model with(vd::rule<T> rule) &&
    {
        add_rule(std::move(rule));
        return std::move(*this);
    }

    basic_model& with(const basic_model& other) &
    {
        if(this != &other) {
            m_rules.insert(m_rules.end(), other.m_rules.begin(), other.m_rules.end());
        }

        return *this;
    }

    basic_model with(const basic_model& other) &&
    {
        if(this != &other) {
            m_rules.insert(m_rules.end(), other.m_rules.begin(), other.m_rules.end());
        }

        return std::move(*this);
    }

    basic_model& with(std::initializer_list<vd::rule<T>> rules) &
    {
        m_rules.insert(m_rules.end(), rules.begin(), rules.end());
        return *this;
    }

    basic_model with(std::initializer_list<vd::rule<T>> rules) &&
    {
        m_rules.insert(m_rules.end(), rules.begin(), rules.end());
        return std::move(*this);
    }

private:
    std::vector<vd::rule<T>> m_rules;
};

template<typename T>
struct basic_bound_model {
    using const_pointer_type = std::add_pointer_t<std::add_const_t<T>>;
    using const_reference_type = std::add_lvalue_reference_t<std::add_const_t<T>>;

    using model_type = basic_model<T>;

    vd::result is_valid() const
    {
        return m_model.is_valid(m_object);
    }

    void die_if_failed() const
    {
        m_model.die_if_failed(m_object);
    }

    // bind stores a non-owning reference — the model must outlive the bound instance.
    static basic_bound_model bind(const model_type& model, const_pointer_type object)
    {
        return basic_bound_model(model, object);
    }

private:
    basic_bound_model(const model_type& model, const_pointer_type object) : m_model(model), m_object(object)
    {
    }

    const model_type& m_model;
    const_pointer_type m_object;
};

template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
vd::result basic_model<T>::is_valid(const_pointer_type object) const
{
    if(object == nullptr) {
        return vd::result(false);
    }

    vd::result final_result(true);
    for(const auto& r : m_rules) {
        vd::result res = r(object);
        final_result.with_other(res);
    }

    return final_result;
}
} // namespace vd

namespace vd
{
template<typename T, typename... Args>
requires(std::same_as<std::decay_t<Args>, T> && ...)
bool validate_many(const basic_model<T>& model, Args&&... objects)
{
    return (model.is_valid(objects).is_valid && ...);
}

template<typename T>
bool validate_many(const basic_model<T>& model, const std::vector<T>& objects)
{
    return std::ranges::all_of(objects, [&model](const T& obj) {
        return model.is_valid(obj).is_valid;
    });
}

template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<T*>& object_ptrs)
{
    return std::ranges::all_of(object_ptrs, [&model](const T* obj) {
        return model.is_valid(obj).is_valid;
    });
}

template<typename T>
requires(!std::is_pointer_v<T> && !std::is_reference_v<T>)
bool validate_many(const basic_model<T>& model, const std::vector<const T*>& object_ptrs)
{
    return std::ranges::all_of(object_ptrs, [&model](const T* obj) {
        return model.is_valid(obj).is_valid;
    });
}
} // namespace vd
#endif // VD_BASIC_MODEL_HXX
