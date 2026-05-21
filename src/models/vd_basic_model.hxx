#pragma once

#ifndef VD_BASIC_MODEL_HXX
#define VD_BASIC_MODEL_HXX

#include <algorithm>
#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <variant>
#include <vector>

#include "models/vd_rule.hxx"

#include "assert/assert.hxx"

#include "utils/vd_overload.hxx"

namespace vd
{
template<typename T>
struct basic_bound_model;

template<typename T>
struct basic_model {
    using const_pointer_type = std::add_pointer_t<std::add_const_t<T>>;
    using const_reference_type = std::add_lvalue_reference_t<std::add_const_t<T>>;

    basic_model() = default;
    basic_model(std::initializer_list<vd::rule<T>> rules) : m_rules(rules)
    {
    }

    bool is_valid(const_pointer_type object) const;

    bool is_valid(const_reference_type object) const
    {
        return is_valid(std::addressof(object));
    }

    basic_bound_model<T> bind(const_pointer_type object) const
    {
        return basic_bound_model<T>::bind(*this, object);
    }

    basic_bound_model<T> bind(const_reference_type object) const
    {
        return bind(std::addressof(object));
    }

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
        m_rules.insert(m_rules.end(), other.m_rules.begin(), other.m_rules.end());
        return *this;
    }

    basic_model with(const basic_model& other) &&
    {
        m_rules.insert(m_rules.end(), other.m_rules.begin(), other.m_rules.end());
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

    bool is_valid() const
    {
        return m_model.is_valid(m_object);
    }

    // bind stores a non-owning reference — the model must outlive the bound instance.
    static basic_bound_model bind(const model_type& model, const_pointer_type object)
    {
        return basic_bound_model(model, object);
    }

private:
    basic_bound_model(const model_type& model, const_pointer_type object)
        : m_model(model), m_object(object)
    {
        vd::require(object != nullptr, "Object pointer cannot be null");
    }

    const model_type& m_model;
    const_pointer_type m_object;
};

template<typename T>
bool basic_model<T>::is_valid(const_pointer_type object) const
{
    for(const auto& r : m_rules) {
        if(!r(*object))
            return false;
    }
    return true;
}
} // namespace vd

#endif // VD_BASIC_MODEL_HXX
