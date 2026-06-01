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
    using pointer_type = T;
    using const_pointer_type = std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>;
    using object_type = std::remove_pointer_t<T>;

    constexpr not_null(std::nullptr_t) = delete;

    constexpr not_null(pointer_type ptr) : m_ptr(ptr)
    {
        vd::ct_require<vd::assertion_exception>(m_ptr != nullptr, "not_null cannot be constructed with nullptr");
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

    constexpr pointer_type get() const
    {
        return m_ptr;
    }

    constexpr const object_type& operator*() const
    {
        return *m_ptr;
    }

    constexpr object_type& operator*()
    {
        return *m_ptr;
    }

    constexpr const_pointer_type operator->() const
    {
        return m_ptr;
    }

    constexpr pointer_type operator->()
    {
        return m_ptr;
    }

    constexpr operator pointer_type() const
    {
        return m_ptr;
    }

private:
    pointer_type m_ptr;
};
} // namespace vd

#endif // VD_NOT_NULL_HXX
