#pragma once

#ifndef VD_NOT_NULL_HXX
#define VD_NOT_NULL_HXX

#include <type_traits>

#include "assert/vd_assert.hxx"

#include "core/vd_exception.hxx"

namespace vd
{
template<typename T>
requires std::is_pointer_v<T>
struct not_null final {
    using value_type = std::remove_cv_t<std::remove_pointer_t<T>>;
    using pointer_type = T;
    using const_pointer_type = const value_type*;
    using object_type = std::remove_pointer_t<T>;
    using reference_type = object_type&;
    using const_reference_type = const value_type&;

    constexpr not_null(std::nullptr_t) = delete;

    constexpr not_null(pointer_type ptr) : m_ptr(ptr)
    {
        vd::ct_require<vd::assertion_exception>(m_ptr != nullptr, "not_null cannot be constructed with nullptr");
    }

    template<typename U>
    requires std::is_convertible_v<U, T>
    constexpr not_null(const not_null<U>& other) noexcept : m_ptr(other.get())
    {
    }

    constexpr not_null(const not_null&) = default;
    constexpr not_null(not_null&&) = default;

    constexpr not_null& operator=(const not_null&) = default;
    constexpr not_null& operator=(not_null&&) = default;

    constexpr not_null& operator=(std::nullptr_t) = delete;

    constexpr not_null& operator=(pointer_type ptr)
    {
        vd::ct_require<vd::assertion_exception>(ptr != nullptr, "not_null cannot be assigned nullptr");
        m_ptr = ptr;
        return *this;
    }

    constexpr pointer_type get() const noexcept
    {
        return m_ptr;
    }

    constexpr const_reference_type operator*() const noexcept
    {
        return *m_ptr;
    }

    constexpr reference_type operator*() noexcept
    requires(!std::is_const_v<object_type>)
    {
        return *m_ptr;
    }

    constexpr const_pointer_type operator->() const noexcept
    {
        return m_ptr;
    }

    constexpr pointer_type operator->() noexcept
    requires(!std::is_const_v<object_type>)
    {
        return m_ptr;
    }

    constexpr operator pointer_type() const noexcept
    {
        return m_ptr;
    }

    constexpr operator const_pointer_type() const noexcept
    requires(!std::is_const_v<object_type>)
    {
        return m_ptr;
    }

private:
    pointer_type m_ptr;
};
} // namespace vd

#endif // VD_NOT_NULL_HXX
